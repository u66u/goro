#include "coro.h"
#include <stdio.h>
#include <unistd.h>

void math(int count, double start) {
    printf("Task %d started\n", count);
    double val = start;
    for (int i = 0; i < 3; i++) {
        val *= 1.5;
        printf("Task %d: val = %f\n", count, val);
        coroutine_yield();
    }
    printf("Task %d done\n", count);
}

typedef struct {
    int id;
    char* name;
} Config;

void complex_struct(Config* c) { printf("Config: %d, %s\n", c->id, c->name); }

int main() {
    scheduler_init(4, 100);

    for (int i = 0; i < 5; i++) {
        GO(math, i, (double)i + 0.5);
    }

    Config c = {.id = 999, .name = "Master"};
    GO(complex_struct, &c);

    scheduler_wait();
    return 0;
}