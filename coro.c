#define _GNU_SOURCE
#include "coro.h"
#include "stack_pool.h"
#include <immintrin.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct Coroutine {
    struct Coroutine* next;
    void* rsp;
    void* stack_base;
    void (*func)(void*);
    void* arg;
    bool finished;
} Coroutine;

typedef struct {
    Coroutine* head;
    Coroutine* tail;
    pthread_spinlock_t lock;
    stack_pool_t* pool;
    int active_tasks;
    bool running;
} Scheduler;

static Scheduler g_sched;

static __thread void* t_worker_rsp = NULL;
static __thread Coroutine* t_current_co = NULL;

void __attribute__((naked)) asm_swap_context(void** prev_rsp, void* next_rsp) {
    asm volatile("    pushq %rbx\n"
                 "    pushq %rbp\n"
                 "    pushq %r12\n"
                 "    pushq %r13\n"
                 "    pushq %r14\n"
                 "    pushq %r15\n"
                 "    subq $8, %rsp\n"   // Alignment padding
                 "    subq $512, %rsp\n" // FPU space
                 "    fxsave (%rsp)\n"
                 "    movq %rsp, (%rdi)\n"
                 "    movq %rsi, %rsp\n"
                 "    fxrstor (%rsp)\n"
                 "    addq $512, %rsp\n"
                 "    addq $8, %rsp\n" // Alignment padding
                 "    popq %r15\n"
                 "    popq %r14\n"
                 "    popq %r13\n"
                 "    popq %r12\n"
                 "    popq %rbp\n"
                 "    popq %rbx\n"
                 "    ret\n");
}

void __attribute__((naked)) asm_entry_point(void) {
    asm volatile("   movq %r13, %rdi\n" // Move arg to 1st argument register
                 "   call *%r12\n"      // Call func
                 "   call coroutine_finish\n");
}

void coroutine_finish() {
    t_current_co->finished = true;
    pthread_spin_lock(&g_sched.lock);
    g_sched.active_tasks--;
    pthread_spin_unlock(&g_sched.lock);

    asm_swap_context(&t_current_co->rsp, t_worker_rsp);
}

void coroutine_yield() {
    pthread_spin_lock(&g_sched.lock);
    t_current_co->next = NULL;
    if (g_sched.tail)
        g_sched.tail->next = t_current_co;
    else
        g_sched.head = t_current_co;
    g_sched.tail = t_current_co;
    pthread_spin_unlock(&g_sched.lock);

    asm_swap_context(&t_current_co->rsp, t_worker_rsp);
}

void* worker_thread(void* arg) {
    (void)arg;
    while (g_sched.running) {
        Coroutine* co = NULL;

        pthread_spin_lock(&g_sched.lock);
        if (g_sched.head) {
            co = g_sched.head;
            g_sched.head = co->next;
            if (!g_sched.head)
                g_sched.tail = NULL;
        }
        pthread_spin_unlock(&g_sched.lock);

        if (co) {
            t_current_co = co;
            asm_swap_context(&t_worker_rsp, co->rsp);

            if (co->finished) {
                stack_pool_return(g_sched.pool, co->stack_base);
                free(co);
            }
        } else {
            // Check if we should exit (naive)
            if (g_sched.active_tasks == 0)
                _mm_pause();
            else
                _mm_pause();
        }
    }
    return NULL;
}

void scheduler_spawn(void (*f)(void*), void* arg) {
    Coroutine* c = malloc(sizeof(Coroutine));
    c->func = f;
    c->arg = arg;
    c->finished = false;

    c->stack_base = stack_pool_get(g_sched.pool);
    if (!c->stack_base) {
        fprintf(stderr, "Pool exhausted!\n");
        free(c);
        return;
    }

    void** sp = (void**)((char*)c->stack_base + g_sched.pool->stack_size);

    sp = (void**)((uintptr_t)sp & ~0xF); // Align 16
    *(--sp) = (void*)asm_entry_point;    // instruction pointer (rip)
    *(--sp) = 0;                         // rbx
    *(--sp) = 0;                         // rbp
    *(--sp) = f;                         // r12 (func)
    *(--sp) = arg;                       // r13 (arg)
    *(--sp) = 0;                         // r14
    *(--sp) = 0;                         // r15
    *(--sp) = 0;                         // padding for fxsave alignment

    sp = (void**)((char*)sp - 512); // FPU space
    memset(sp, 0, 512);

    c->rsp = sp;

    pthread_spin_lock(&g_sched.lock);
    c->next = NULL;
    if (g_sched.tail)
        g_sched.tail->next = c;
    else
        g_sched.head = c;
    g_sched.tail = c;
    g_sched.active_tasks++;
    pthread_spin_unlock(&g_sched.lock);
}

void scheduler_init(int threads, int stack_count) {
    memset(&g_sched, 0, sizeof(g_sched));
    pthread_spin_init(&g_sched.lock, PTHREAD_PROCESS_PRIVATE);

    g_sched.pool = stack_pool_create(stack_count, DEFAULT_STACK_SIZE);
    if (!g_sched.pool) {
        perror("Failed to create stack pool");
        exit(1);
    }

    g_sched.running = true;

    for (int i = 0; i < threads; i++) {
        pthread_t t;
        pthread_create(&t, NULL, worker_thread, NULL);
        pthread_detach(t);
    }
}

void scheduler_wait() {
    while (1) {
        pthread_spin_lock(&g_sched.lock);
        int pending = g_sched.active_tasks;
        pthread_spin_unlock(&g_sched.lock);

        if (pending == 0)
            break;
        usleep(1000);
    }
}