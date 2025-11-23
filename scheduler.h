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


#if defined(__GNUC__) || defined(__clang__)
#define GO(user_func, ...)                                                     \
    do {                                                                       \
        struct {                                                               \
            __auto_type __VA_ARGS__;                                           \
        } args_struct = {__VA_ARGS__};                                         \
                                                                               \
        /* nested function in macros requires a GCC/Clang extension */         \
        void go_wrapper(void* arg_bundle) {                            \
            typeof(args_struct)* p_args = arg_bundle;                  \
            /* call the original user function with the unpacked arguments */  \
            user_func(p_args->__VA_ARGS__);                                    \
            free(p_args);                                                      \
        }                                                                      \
                                                                               \
        void* args_alloc = malloc(sizeof(args_struct));                         \
        if (args_alloc) {                                                       \
            memcpy(args_alloc, &args_struct, sizeof(args_struct));              \
            /* use a global scheduler instance here */                         \
            coroutine_t* coro =                                                \
                coro_create(go_wrapper, args_alloc, g_scheduler->stack_pool);   \
            if (coro) {                                                        \
                scheduler_add(g_scheduler, coro);                              \
            } else {                                                           \
                free(args_alloc);                                               \
            }                                                                  \
        }                                                                      \
    } while (0)
#endif


#endif