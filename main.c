#include "arena.h"

int main() {
    size_t num_stacks = 100;
    stack_pool_t* p = stack_pool_create(num_stacks, 1024*64);
    for (int i = 0; i < num_stacks; i++) {
        void* node = stack_pool_get(p);
        stack_pool_return(p, node);
    }

}