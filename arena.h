#include <sys/mman.h> 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define DEFAULT_STACK_SIZE (1024 * 64) // 64KB


typedef struct free_block {
  struct free_block *next;
} free_block_t;


typedef struct {
  void *pool_start;
  free_block_t *freelist_head;
  size_t size;
} stack_pool_t;


stack_pool_t* stack_pool_create(size_t num_stacks, size_t stack_size) {
     if (num_stacks < 1) {
          fprintf(stderr, "Cannot create a pool with 0 stacks.\n");
          return NULL;
     }

     if (stack_size == 0) stack_size = DEFAULT_STACK_SIZE;

     long page_size = getpagesize();
     if (page_size == -1) {
          perror("getpagesize failed");
          return NULL;
     }

     size_t chunk_size = page_size + stack_size;
     size_t pool_size = num_stacks * chunk_size;

     void* mem = mmap(NULL, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
     if (mem == MAP_FAILED) {
          perror("mmap failed to allocate stack pool");
          return NULL; 
     }

     char* iter = (char*)mem;
     free_block_t* head = NULL;

     for (size_t i = 0; i < num_stacks; i++) {
          if (mprotect(iter, page_size, PROT_NONE) == -1) { 
               perror("mprotect failed to set a guard page");
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
     if (!pool) {
          fprintf(stderr, "Failed to allocate stack_pool struct.\n");
          munmap(mem, pool_size);
          return NULL;
     }

     pool->pool_start = mem;
     pool->size = pool_size;
     pool->freelist_head = head;

     return pool;
}

void stack_pool_destroy(stack_pool_t* pool) {
    if (pool) {
        munmap(pool->pool_start, pool->size);
        free(pool);
    }
}

void* stack_pool_get(stack_pool_t *pool) {
     if (!pool || !pool->freelist_head) { 
          fprintf(stderr, "could not get a free node from the pool because the pool is NULL or the node is NULL. Make sure you're not over pool's capacity");
          return NULL;
     }

     free_block_t* node = pool->freelist_head;
     pool -> freelist_head = node->next;
     return (void*) node;
}

void stack_pool_return(stack_pool_t *pool, void *stack) {
     if (!pool || !stack) {  
          fprintf(stderr, "could not return stack to pool because either pool or stack is NULL");
          return;
     }

     free_block_t* node = (free_block_t*) stack;
     node->next = pool->freelist_head;
     pool->freelist_head = node;
}