#define _GNU_SOURCE
#include "coro.h"
#include "coro_internal.h"
#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Scheduler g_sched;
__thread void* t_worker_rsp = NULL;
__thread Coroutine* t_current_co = NULL;
__thread LocalQueue* t_local_q = NULL;

void coroutine_finish(void);

void __attribute__((naked)) coro_swap_context(void** prev_rsp, void* next_rsp) {
    asm volatile("    pushq %rbx\n"
                 "    pushq %rbp\n"
                 "    pushq %r12\n"
                 "    pushq %r13\n"
                 "    pushq %r14\n"
                 "    pushq %r15\n"
                 "    subq $8, %rsp\n"   // Align 16 (0x...8 -> 0x...0)
                 "    subq $512, %rsp\n" // FPU space
                 "    fxsave (%rsp)\n"
                 "    movq %rsp, (%rdi)\n"
                 "    movq %rsi, %rsp\n"
                 "    fxrstor (%rsp)\n"
                 "    addq $512, %rsp\n"
                 "    addq $8, %rsp\n" // Undo align
                 "    popq %r15\n"
                 "    popq %r14\n"
                 "    popq %r13\n"
                 "    popq %r12\n"
                 "    popq %rbp\n"
                 "    popq %rbx\n"
                 "    ret\n");
}

void coroutine_finish(void) {
    t_current_co->finished = true;
    atomic_fetch_sub(&g_sched.active_tasks, 1);
    coro_swap_context(&t_current_co->rsp, t_worker_rsp);
}

void __attribute__((naked)) coro_entry_point(void) {
    asm volatile("   movq %r13, %rdi\n" // move arg
                 "   call *%r12\n"      // call Func
                 "   call coroutine_finish\n");
}

CORO_API void coro_yield(void) {
    coro_sched_local_q_push(t_local_q, t_current_co);
    coro_swap_context(&t_current_co->rsp, t_worker_rsp);
}

static void* worker_thread(void* arg) {
    size_t id = (size_t)arg;
    t_local_q = &g_sched.queues[id];

    while (g_sched.running) {
        Coroutine* co = NULL;

        co = coro_sched_local_q_pop(t_local_q);
        if (!co)
            co = coro_sched_global_q_pop();

        // try to steal work
        if (!co) {
            size_t victim =
                rand() % g_sched.thread_count; // maybe we want to be smarter
            if (victim != id) {
                co = coro_sched_steal(&g_sched.queues[victim]);
            }
        }

        if (co) {
            t_current_co = co;
            coro_swap_context(&t_worker_rsp, co->rsp);

            if (co->finished) {
                coro_pool_return(g_sched.pool, co->stack_base);
                free(co);
            }
        } else {
            if (atomic_load(&g_sched.active_tasks) == 0)
                _mm_pause();
            else
                _mm_pause();
        }
    }
    return NULL;
}

CORO_API void coro_spawn(void (*f)(void*), void* arg) {
    Coroutine* c = malloc(sizeof(Coroutine));
    c->func = f;
    c->arg = arg;
    c->finished = false;

    c->stack_base = coro_pool_get(g_sched.pool);
    if (!c->stack_base) {
        fprintf(stderr, "CORO: Out of stacks!\n");
        free(c);
        return;
    }

    // stack init
    void** sp = (void**)((char*)c->stack_base + g_sched.pool->stack_size);
    sp = (void**)((uintptr_t)sp & ~0xF);
    *(--sp) = (void*)coro_entry_point;
    *(--sp) = 0;   // RBX
    *(--sp) = 0;   // RBP
    *(--sp) = f;   // R12
    *(--sp) = arg; // R13
    *(--sp) = 0;   // R14
    *(--sp) = 0;   // R15
    *(--sp) = 0;   // Padding

    sp = (void**)((char*)sp - 512);
    memset(sp, 0, 512);

    c->rsp = sp;

    atomic_fetch_add(&g_sched.active_tasks, 1);

    if (t_local_q) {
        coro_sched_local_q_push(t_local_q, c);
    } else {
        coro_sched_global_q_push(c);
    }
}

CORO_API void coro_scheduler_init(int thread_count, int stack_count) {
    memset(&g_sched, 0, sizeof(g_sched));

    pthread_spin_init(&g_sched.global_lock, PTHREAD_PROCESS_PRIVATE);
    g_sched.thread_count = thread_count;
    g_sched.queues = malloc(sizeof(LocalQueue) * thread_count);

    for (int i = 0; i < thread_count; i++) {
        atomic_init(&g_sched.queues[i].head, 0);
        atomic_init(&g_sched.queues[i].tail, 0);
        g_sched.queues[i].id = i;
    }

    g_sched.pool = coro_pool_create(stack_count, CORO_DEFAULT_STACK_SIZE);
    if (!g_sched.pool)
        exit(1);

    g_sched.running = true;

    for (int i = 0; i < thread_count; i++) {
        pthread_t t;
        pthread_create(&t, NULL, worker_thread, (void*)(uintptr_t)i);
        pthread_detach(t);
    }
}

CORO_API void coro_wait(void) {
    while (atomic_load(&g_sched.active_tasks) > 0) {
        usleep(1000);
    }
}