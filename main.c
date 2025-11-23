#include "scheduler.h"
#include <stdio.h>

void worker(int id, int count) {
    for (int i = 0; i < count; i++) {
        printf("Worker %d: Iteration %d\n", id, i);
        scheduler_yield();
    }
    printf("Worker %d: DONE\n", id);
}

void printer(char* msg, int num) {
    printf("Printer: %s (Val: %d)\n", msg, num);
}

void simple(void) { printf("Simple task running\n"); }

int main() {
    scheduler_init(10);

    printf("Main: Spawning tasks...\n");

    GO(worker, 1, 3);
    GO(worker, 2, 2);
    GO(printer, "Hello World", 42);
    GO(simple);

    scheduler_start();

    scheduler_cleanup();
    return 0;
}