#include "coro_internal.h"
#include <stdlib.h>

void coro_sched_global_q_push(Coroutine* c) {
    pthread_spin_lock(&g_sched.global_lock);
    c->next = NULL;
    if (g_sched.global_tail)
        g_sched.global_tail->next = c;
    else
        g_sched.global_head = c;
    g_sched.global_tail = c;
    pthread_spin_unlock(&g_sched.global_lock);
}

Coroutine* coro_sched_global_q_pop(void) {
    pthread_spin_lock(&g_sched.global_lock);
    Coroutine* c = g_sched.global_head;
    if (c) {
        g_sched.global_head = c->next;
        if (!g_sched.global_head)
            g_sched.global_tail = NULL;
    }
    pthread_spin_unlock(&g_sched.global_lock);
    return c;
}

void coro_sched_local_q_push(LocalQueue* q, Coroutine* c) {
    uint32_t h = atomic_load_explicit(&q->head, memory_order_relaxed);
    uint32_t t = atomic_load_explicit(&q->tail, memory_order_acquire);

    if ((h - t) >= PER_THREAD_Q_CAP) {
        coro_sched_global_q_push(c);
        return;
    }

    atomic_store_explicit(&q->buffer[h & Q_MASK], c, memory_order_relaxed);
    atomic_store_explicit(&q->head, h + 1, memory_order_release);
}

Coroutine* coro_sched_local_q_pop(LocalQueue* q) {
    uint32_t h = atomic_load_explicit(&q->head, memory_order_relaxed) - 1;
    atomic_store_explicit(&q->head, h, memory_order_relaxed);

    atomic_thread_fence(memory_order_seq_cst);

    uint32_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);

    if (h < t) {
        atomic_store_explicit(&q->head, t, memory_order_relaxed);
        return NULL;
    }

    Coroutine* c =
        atomic_load_explicit(&q->buffer[h & Q_MASK], memory_order_relaxed);
    if (h > t)
        return c;

    // conflict: h == t. We are fighting for the last item.
    if (!atomic_compare_exchange_strong_explicit(
            &q->tail, &t, t + 1, memory_order_seq_cst, memory_order_relaxed)) {
        // LOST RACE: Thief stole it. Queue is empty.
        // We must restore head to t + 1 (consistent with empty state)
        atomic_store_explicit(&q->head, t + 1, memory_order_relaxed);
        return NULL;
    }

    // WON RACE: We got it.
    atomic_store_explicit(&q->head, t + 1, memory_order_relaxed);
    return c;
}

Coroutine* coro_sched_steal(LocalQueue* victim) {
    uint32_t t = atomic_load_explicit(&victim->tail, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    uint32_t h = atomic_load_explicit(&victim->head, memory_order_acquire);

    if (t >= h)
        return NULL;

    Coroutine* c =
        atomic_load_explicit(&victim->buffer[t & Q_MASK], memory_order_relaxed);

    if (!atomic_compare_exchange_strong_explicit(&victim->tail, &t, t + 1,
                                                 memory_order_seq_cst,
                                                 memory_order_relaxed)) {
        return NULL;
    }
    return c;
}