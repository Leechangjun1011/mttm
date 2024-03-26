CC = gcc
CFLAGS = -g -Wall
INCLUDES = -I../../linux-5.15.145/usr/include/
all: run_bench
 
run_bench: run_bench.c
	$(CC) run_bench.c -o run_bench

clean:
	rm -rf run_bench 
