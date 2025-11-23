#ifndef CORO_IMPLEMENTATION
#define CORO_IMPLEMENTATION

#include "arena.h"
#include <ucontext.h>

typedef enum { CORO_READY, CORO_RUNNING, CORO_DEAD } coro_status_t;

typedef void (*coro_func_t)(void*);

typedef struct coroutine {
    ucontext_t context;
    void* stack;
    struct coroutine* next;
    coro_func_t entry_func;
    void* arg;
    coro_status_t status;
    int id;
} coroutine_t;

coroutine_t* coro_create(void (*func)(void*), void* arg,
                         void (*trampoline)(void), stack_pool_t* pool) {
    if (!pool)
        return NULL;

    coroutine_t* coro = (coroutine_t*)malloc(sizeof(coroutine_t));
    if (!coro)
        return NULL;

    coro->stack = stack_pool_get(pool);
    if (!coro->stack) {
        free(coro);
        return NULL;
    }

    if (getcontext(&coro->context) == -1) {
        stack_pool_return(pool, coro->stack);
        free(coro);
        return NULL;
    }

    coro->context.uc_stack.ss_sp = coro->stack;
    coro->context.uc_stack.ss_size = pool->stack_size;
    coro->context.uc_link = NULL;

    coro->entry_func = func;
    coro->arg = arg;

    // configure the context to jump to the trampoline
    makecontext(&coro->context, trampoline, 0);

    return coro;
}


#endif