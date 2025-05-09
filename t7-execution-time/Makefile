CC = gcc
CFLAGS = -Wall -g

.PHONY: all clean

all: test1-same-priority test2-distinct-priority

test1-same-priority: test1-same-priority.o ppos_core.o queue.o
	$(CC) -o $@ $^

test2-distinct-priority: test2-distinct-priority.o ppos_core.o queue.o
	$(CC) -o $@ $^

test1-same-priority.o: test1-same-priority.c
	$(CC) $(CFLAGS) -c $<

test2-distinct-priority.o: test2-distinct-priority.c
	$(CC) $(CFLAGS) -c $<

ppos_core.o: ppos_core.c
	$(CC) $(CFLAGS) -c $<

queue.o: queue.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o test1-same-priority test2-distinct-priority