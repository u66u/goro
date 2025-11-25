#include "src/coro.h"
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void task_print(int id, double val) {
    printf("[Task %d] Value: %.2f (running on thread)\n", id, val);
}

typedef struct {
    int x, y;
} Point;

void task_struct(Point* p) {
    printf("[Struct Task] Point: (%d, %d)\n", p->x, p->y);
}

void task_heavy(int id) {
    for (int i = 0; i < 3; i++) {
        printf("[Task %d] Working step %d...\n", id, i);
        coro_yield();
    }
    printf("[Task %d] Finished.\n", id);
}

atomic_int g_counter = 0;

void task_increment(void* arg) {
    (void)arg;

    atomic_fetch_add(&g_counter, 1);
}

int main(void) {
    int num_threads = 1;


    int stack_pool_size = 5000;

    printf("Initializing Scheduler with %d threads and %d stacks...\n",
           num_threads, stack_pool_size);

    coro_init(num_threads, stack_pool_size);

    printf("\n--- 1. Testing Arguments ---\n");
    GO(task_print, 1, 3.14);
    GO(task_print, 2, 6.28);


    Point p = {10, 20};
    GO(task_struct, &p);

    printf("\n--- 2. Testing Yielding ---\n");
    GO(task_heavy, 100);
    GO(task_heavy, 101);


    coro_wait();

    printf("\n--- 3. Stress Test (Work Stealing) ---\n");
    printf("Spawning 4000 tasks...\n");

    clock_t start = clock();

    for (int i = 0; i < 4000; i++) {

        coro_spawn(task_increment, NULL);
    }

    coro_wait();

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Final Counter: %d (Expected: 4000)\n", atomic_load(&g_counter));
    printf("Time taken: %f seconds\n", time_taken);

    return 0;
}