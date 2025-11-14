#include <ucontext.h>

typedef struct {
    ucontext_t context;
    void* stack;
    enum {  CORO_RUNNABLE, 
            CORO_WAITING, 
            CORO_DEAD 
          } status;
} coroutine_t;

coroutine_t* coro_create (void (*func)(void), void* stack_pool) {};
