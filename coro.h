#ifndef CORO_IMPLEMENTATION
#define CORO_IMPLEMENTATION

#include "arena.h"
#include <ucontext.h>

typedef struct {
    ucontext_t context;
    void* stack;
    enum { CORO_RUNNABLE, 
           CORO_WAITING, 
           CORO_DEAD,
           CORO_READY
     } status;
} coroutine_t;

coroutine_t* coro_create(void (*func)(void*), void* arg, stack_pool_t* pool) {
    if (!pool) {
        fprintf(stderr, "coro_create: stack pool is NULL.\n");
        return NULL;
    }

    coroutine_t* coro = (coroutine_t*)malloc(sizeof(coroutine_t));
    if (!coro) {
        perror("coro_create: failed to malloc coroutine_t");
        return NULL;
    }

    coro->stack = stack_pool_get(pool);
    if (!coro->stack) {
        fprintf(
            stderr,
            "coro_create: failed to get stack from pool (pool is empty).\n");
        free(coro);
        return NULL;
    }

    if (getcontext(&coro->context) == -1) {
        perror("getcontext failed");

        stack_pool_return(pool, coro->stack);
        free(coro);
        return NULL;
    }

    coro->context.uc_stack.ss_sp = coro->stack;
    coro->context.uc_stack.ss_size = pool->stack_size;

    coro->context.uc_link = NULL;

    coro->status = CORO_READY;

    makecontext(&coro->context, (void (*)(void))func, 1, arg);

    return coro;
}

#endif