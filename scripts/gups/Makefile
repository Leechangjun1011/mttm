CC = gcc
CFLAGS = -g -Wall -O3 -Winline
#CFLAGS = -g3 -Wall -O0
INCLUDES = -I../../linux-5.15.95/usr/include
LIBS = -lm -lpthread

all: gups-1 gups-2 gups-3

gups-1: gups-1.o timer.o
	$(CC) $(CFLAGS) $(INCLUDES) -o gups-1 gups-1.o zipf.o timer.o $(LIBS)

gups-2: gups-2.o timer.o
	$(CC) $(CFLAGS) $(INCLUDES) -o gups-2 gups-2.o zipf.o timer.o $(LIBS)

gups-3: gups-3.o timer.o
	$(CC) $(CFLAGS) $(INCLUDES) -o gups-3 gups-3.o zipf.o timer.o $(LIBS)

gups-1.o: gups-1.c zipf.c gups.h
	$(CC) $(CFLAGS) $(INCLUDES) -c gups-1.c zipf.c

gups-2.o: gups-2.c zipf.c gups.h
	$(CC) $(CFLAGS) $(INCLUDES) -c gups-2.c zipf.c

gups-3.o: gups-3.c zipf.c gups.h
	$(CC) $(CFLAGS) $(INCLUDES) -c gups-3.c zipf.c

timer.o: timer.c timer.h
	$(CC) $(CFLAGS) $(INCLUDES) -c timer.c

clean:
	rm -rf *.o gups-1 gups-2 gups-3
