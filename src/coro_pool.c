#define _GNU_SOURCE
#include "coro_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

stack_pool_t* coro_pool_create(size_t num_stacks, size_t max_stack_size) {
    if (num_stacks < 1)
        return NULL;
    if (max_stack_size < 1)
        max_stack_size = CORO_DEFAULT_STACK_SIZE;

    long page_size = getpagesize();
    size_t chunk_size = page_size + max_stack_size;
    size_t pool_size = num_stacks * chunk_size;

    /* we use MAP_NORESERVE to allocate virtual pages without reserving
    actual swap space to "dynamically" grow coroutines' stacks without malloc.
    Coroutines' stack size is still limited by the passed stack_size parameter
    or by CORO_DEFAULT_STACK_SIZE  */
    void* mem =
        mmap(NULL, pool_size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_NORESERVE, -1, 0);
    if (mem == MAP_FAILED)
        return NULL;

    char* iter = (char*)mem;
    free_block_t* head = NULL;

    for (size_t i = 0; i < num_stacks; i++) {
        // Guard page
        if (mprotect(iter, page_size, PROT_NONE) == -1) {
            munmap(mem, pool_size);
            return NULL;
        }

        char* usable_chunk = iter + page_size;
        free_block_t* new_node = (free_block_t*)usable_chunk;
        new_node->next = head;
        head = new_node;

        iter += chunk_size;
    }

    stack_pool_t* pool = malloc(sizeof(stack_pool_t));
    pool->pool_start = mem;
    pool->stack_size = max_stack_size;
    pool->pool_size = pool_size;
    pool->freelist_head = head;
    pthread_spin_init(&pool->lock, PTHREAD_PROCESS_PRIVATE);

    return pool;
}

void* coro_pool_get(stack_pool_t* pool) {
    pthread_spin_lock(&pool->lock);
    if (!pool->freelist_head) {
        pthread_spin_unlock(&pool->lock);
        return NULL;
    }
    free_block_t* node = pool->freelist_head;
    pool->freelist_head = node->next;
    pthread_spin_unlock(&pool->lock);
    return (void*)node;
}

void coro_pool_return(stack_pool_t* pool, void* stack) {

    /* free ONLY physical pages, keeps the virtual address mapped for reuse in
     the stack pool */
    madvise(stack, pool->stack_size, MADV_DONTNEED);

    pthread_spin_lock(&pool->lock);
    free_block_t* node = (free_block_t*)stack;
    node->next = pool->freelist_head;
    pool->freelist_head = node;
    pthread_spin_unlock(&pool->lock);
}
