#ifndef COROUTINE_H
#define COROUTINE_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

void scheduler_init(int threads, int stack_count);

void scheduler_wait();

void coroutine_yield();

void scheduler_spawn(void (*f)(void*), void* arg);


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

// structs can only be passed by pointer
#define GO(FUNC, ...)                                                          \
    _GO_CAT(_GO_DISPATCH_, _GO_COUNT_ARGS(FUNC, ##__VA_ARGS__))                \
    (FUNC, ##__VA_ARGS__)

#endif