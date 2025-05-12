CC = gcc
CFLAGS = -g -Wall
INCLUDES = -I../../linux-5.15.145/usr/include/
all: run_bench run_bench_vtmm memeater
 
run_bench: run_bench.c
	$(CC) run_bench.c -o run_bench

run_bench_vtmm: run_bench_vtmm.c
	$(CC) run_bench_vtmm.c -o run_bench_vtmm

memeater: memeater.c
	$(CC) memeater.c -o memeater
clean:
	rm -rf run_bench run_bench_vtmm memeater
