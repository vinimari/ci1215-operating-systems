CC = gcc
CFLAGS = -Wall -g

.PHONY: all clean

all: pingpong-preempcao

pingpong-preempcao: pingpong-preempcao.o ppos_core.o queue.o
	$(CC) -o $@ $^

pingpong-preempcao.o: pingpong-preempcao.c
	$(CC) $(CFLAGS) -c $<

ppos_core.o: ppos_core.c
	$(CC) $(CFLAGS) -c $<

queue.o: queue.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o pingpong-preempcao