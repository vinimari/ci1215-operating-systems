CC = gcc
CFLAGS = -Wall -g

.PHONY: all clean

all: ppos

ppos: main.o ppos_core.o queue.o
	$(CC) -o $@ $^

main.o: main.c
	$(CC) $(CFLAGS) -c $<

ppos_core.o: ppos_core.c
	$(CC) $(CFLAGS) -c $<

queue.o: queue.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o ppos