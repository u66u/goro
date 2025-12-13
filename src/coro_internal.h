#ifndef CORO_INTERNAL_H
#define CORO_INTERNAL_H

#include "coro.h"
#include <assert.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>

#define PER_THREAD_Q_CAP 256
#define Q_MASK (PER_THREAD_Q_CAP - 1)

static_assert((PER_THREAD_Q_CAP > 0) &&
                  ((PER_THREAD_Q_CAP & (PER_THREAD_Q_CAP - 1)) == 0),
              "PER_THREAD_Q_CAP must be a power of 2");

typedef struct Coroutine {
    struct Coroutine* next;
    void* rsp;
    void* stack_base;
    void (*func)(void*);
    void* arg;
    bool finished;
} Coroutine;

typedef struct {
    Coroutine* _Atomic buffer[PER_THREAD_Q_CAP];
    alignas(64) atomic_uint head; // modified by owner
    alignas(64) atomic_uint tail; // modified when work is stolen
    size_t id;
} LocalQueue;

typedef struct free_block {
    struct free_block* next;
} free_block_t;

typedef struct {
    void* pool_start;
    free_block_t* freelist_head;
    size_t stack_size;
    size_t pool_size;
    pthread_spinlock_t lock;
} stack_pool_t;

typedef struct {
    Coroutine* global_head;
    Coroutine* global_tail;
    pthread_spinlock_t global_lock;

    LocalQueue* queues;
    size_t thread_count;
    stack_pool_t* pool;

    _Atomic size_t active_tasks;
    bool running;
} Scheduler;

extern Scheduler g_sched;
extern __thread void* t_worker_rsp;
extern __thread Coroutine* t_current_co;
extern __thread LocalQueue* t_local_q;


stack_pool_t* coro_pool_create(size_t num_stacks, size_t stack_size);
void* coro_pool_get(stack_pool_t* pool);
void coro_pool_return(stack_pool_t* pool, void* stack);

void coro_sched_global_q_push(Coroutine* c);
Coroutine* coro_sched_global_q_pop(void);
void coro_sched_local_q_push(LocalQueue* q, Coroutine* c);
Coroutine* coro_sched_local_q_pop(LocalQueue* q);
Coroutine* coro_sched_steal(LocalQueue* victim);

#endif
