#ifndef ARENA_IMPLEMENTATION
#define ARENA_IMPLEMENTATION

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define DEFAULT_STACK_SIZE (1024 * 64) // 64 kb

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

stack_pool_t* stack_pool_create(size_t num_stacks, size_t stack_size) {
    if (num_stacks < 1)
        return NULL;
    if (stack_size == 0)
        stack_size = DEFAULT_STACK_SIZE;

    long page_size = getpagesize();
    size_t chunk_size = page_size + stack_size;
    size_t pool_size = num_stacks * chunk_size;

    void* mem = mmap(NULL, pool_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (mem == MAP_FAILED)
        return NULL;

    char* iter = (char*)mem;
    free_block_t* head = NULL;

    for (size_t i = 0; i < num_stacks; i++) {
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

    stack_pool_t* pool = (stack_pool_t*)malloc(sizeof(stack_pool_t));
    pool->pool_start = mem;
    pool->stack_size = stack_size;
    pool->pool_size = pool_size;
    pool->freelist_head = head;
    pthread_spin_init(&pool->lock, PTHREAD_PROCESS_PRIVATE);

    return pool;
}

void* stack_pool_get(stack_pool_t* pool) {
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

void stack_pool_return(stack_pool_t* pool, void* stack) {
    pthread_spin_lock(&pool->lock);
    free_block_t* node = (free_block_t*)stack;
    node->next = pool->freelist_head;
    pool->freelist_head = node;
    pthread_spin_unlock(&pool->lock);
}
#endif