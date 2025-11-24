all:
	gcc -O3 main.c coro.c -lpthread -o main       