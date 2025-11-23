#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "coro.h"
#include <string.h>

typedef struct {
    stack_pool_t* stack_pool;
    coroutine_t* head;
    coroutine_t* tail;
    ucontext_t sched_ctx;
    coroutine_t* current;
    int running_tasks;
    int id_counter;
} scheduler_t;

// global scheduer instance
static scheduler_t* g_scheduler = NULL;

void scheduler_yield(void);

static void scheduler_trampoline(void) {
    scheduler_t* sched = g_scheduler;
    coroutine_t* coro = sched->current;

    if (coro->entry_func) {
        coro->entry_func(coro->arg);
    }

    sched->running_tasks--;
    stack_pool_return(sched->stack_pool, coro->stack);

    free(coro);
    sched->current = NULL;

    setcontext(&sched->sched_ctx);
}

void scheduler_init(size_t stack_count) {
    g_scheduler = (scheduler_t*)malloc(sizeof(scheduler_t));
    g_scheduler->stack_pool = stack_pool_create(stack_count, 0);
    g_scheduler->head = NULL;
    g_scheduler->tail = NULL;
    g_scheduler->current = NULL;
    g_scheduler->running_tasks = 0;
    g_scheduler->id_counter = 0;
}

void scheduler_add(scheduler_t* sched, coroutine_t* coro) {
    if (!sched || !coro)
        return;

    coro->id = ++sched->id_counter;
    coro->next = NULL;

    if (sched->tail) {
        sched->tail->next = coro;
    } else {
        sched->head = coro;
    }
    sched->tail = coro;
    sched->running_tasks++;
}

void scheduler_spawn(void (*func)(void*), void* arg) {
    if (!g_scheduler)
        return;

    coroutine_t* coro =
        coro_create(func, arg, scheduler_trampoline, g_scheduler->stack_pool);
    if (coro) {
        scheduler_add(g_scheduler, coro);
    } else {
        fprintf(stderr, "Failed to spawn task: Out of stacks or memory.\n");
    }
}

void scheduler_start(void) {
    if (!g_scheduler)
        return;

    printf("[Scheduler] Started. Tasks pending: %d\n",
           g_scheduler->running_tasks);

    while (g_scheduler->head != NULL) {
        coroutine_t* coro = g_scheduler->head;
        g_scheduler->head = coro->next;
        if (g_scheduler->head == NULL) {
            g_scheduler->tail = NULL;
        }
        coro->next = NULL;

        g_scheduler->current = coro;

        swapcontext(&g_scheduler->sched_ctx, &coro->context);

        if (g_scheduler->current != NULL) {
            if (g_scheduler->tail) {
                g_scheduler->tail->next = g_scheduler->current;
            } else {
                g_scheduler->head = g_scheduler->current;
            }
            g_scheduler->tail = g_scheduler->current;
            g_scheduler->current = NULL;
        }
    }

    printf("[Scheduler] No more tasks. Exiting.\n");
}

void scheduler_yield(void) {
    scheduler_t* sched = g_scheduler;
    if (!sched || !sched->current)
        return;

    swapcontext(&sched->current->context, &sched->sched_ctx);
}

void scheduler_cleanup(void) {
    if (g_scheduler) {
        stack_pool_destroy(g_scheduler->stack_pool);
        free(g_scheduler);
        g_scheduler = NULL;
    }
}

#define _GO_CAT_IMPL(a, b) a##b
#define _GO_CAT(a, b) _GO_CAT_IMPL(a, b)

#define _GO_COUNT_ARGS(...) _GO_COUNT_IMPL(__VA_ARGS__, 5, 4, 3, 2, 1)
#define _GO_COUNT_IMPL(_1, _2, _3, _4, _5, N, ...) N

#define _GO_DISPATCH_1(FUNC) _GO_0(FUNC)
#define _GO_DISPATCH_2(FUNC, ...) _GO_1(FUNC, __VA_ARGS__)
#define _GO_DISPATCH_3(FUNC, ...) _GO_2(FUNC, __VA_ARGS__)
#define _GO_DISPATCH_4(FUNC, ...) _GO_3(FUNC, __VA_ARGS__)


#define _GO_0(FUNC) scheduler_spawn((void (*)(void*))FUNC, NULL)

#define _GO_1(FUNC, A1)                                                        \
    ({                                                                         \
        typedef __typeof__((A1) + 0) _T1;                                      \
        struct args_1 {                                                        \
            _T1 a;                                                             \
        };                                                                     \
        struct args_1* args = malloc(sizeof(struct args_1));                   \
        args->a = (A1);                                                        \
        void _wrapper(void* p) {                                               \
            struct args_1* u = (struct args_1*)p;                              \
            FUNC(u->a);                                                        \
            free(u);                                                           \
        }                                                                      \
        scheduler_spawn(_wrapper, args);                                       \
    })

#define _GO_2(FUNC, A1, A2)                                                    \
    ({                                                                         \
        typedef __typeof__((A1) + 0) _T1;                                      \
        typedef __typeof__((A2) + 0) _T2;                                      \
        struct args_2 {                                                        \
            _T1 a;                                                             \
            _T2 b;                                                             \
        };                                                                     \
        struct args_2* args = malloc(sizeof(struct args_2));                   \
        args->a = (A1);                                                        \
        args->b = (A2);                                                        \
        void _wrapper(void* p) {                                               \
            struct args_2* u = (struct args_2*)p;                              \
            FUNC(u->a, u->b);                                                  \
            free(u);                                                           \
        }                                                                      \
        scheduler_spawn(_wrapper, args);                                       \
    })

#define _GO_3(FUNC, A1, A2, A3)                                                \
    ({                                                                         \
        typedef __typeof__((A1) + 0) _T1;                                      \
        typedef __typeof__((A2) + 0) _T2;                                      \
        typedef __typeof__((A3) + 0) _T3;                                      \
        struct args_3 {                                                        \
            _T1 a;                                                             \
            _T2 b;                                                             \
            _T3 c;                                                             \
        };                                                                     \
        struct args_3* args = malloc(sizeof(struct args_3));                   \
        args->a = (A1);                                                        \
        args->b = (A2);                                                        \
        args->c = (A3);                                                        \
        void _wrapper(void* p) {                                               \
            struct args_3* u = (struct args_3*)p;                              \
            FUNC(u->a, u->b, u->c);                                            \
            free(u);                                                           \
        }                                                                      \
        scheduler_spawn(_wrapper, args);                                       \
    })

#define GO(FUNC, ...)                                                          \
    _GO_CAT(_GO_DISPATCH_, _GO_COUNT_ARGS(FUNC, ##__VA_ARGS__))                \
    (FUNC, ##__VA_ARGS__)

#endif