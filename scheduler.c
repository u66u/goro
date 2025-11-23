#if defined(__GNUC__) || defined(__clang__)
#define GO(user_func, ...)                                                     \
    do {                                                                       \
        struct {                                                               \
            __auto_type __VA_ARGS__;                                           \
        } args_struct = {__VA_ARGS__};                                         \
                                                                               \
        /* nested function in macros requires a GCC/Clang extension */         \
        void go_wrapper(void* arg_bundle_on_heap) {                            \
            typeof(args_struct)* p_args = arg_bundle_on_heap;                  \
            /* call the original user function with the unpacked arguments */  \
            user_func(p_args->__VA_ARGS__);                                    \
            free(p_args);                                                      \
        }                                                                      \
                                                                               \
        void* heap_args = malloc(sizeof(args_struct));                         \
        if (heap_args) {                                                       \
            memcpy(heap_args, &args_struct, sizeof(args_struct));              \
            /* use a global scheduler instance here */                         \
            coroutine_t* coro =                                                \
                coro_create(go_wrapper, heap_args, g_scheduler->stack_pool);   \
            if (coro) {                                                        \
                scheduler_add(g_scheduler, coro);                              \
            } else {                                                           \
                free(heap_args);                                               \
            }                                                                  \
        }                                                                      \
    } while (0)
#endif