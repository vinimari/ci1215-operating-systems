# Compiladores e flags
CC = gcc
CFLAGS = -Wall -g

# Arquivos do projeto
SRCS = ppos_core.c queue.c pingpong-scheduler.c
OBJS = $(SRCS:.c=.o)
TARGET = pingpong-scheduler

# Regras de compilação
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpeza
clean:
	rm -f $(OBJS) *~ $(TARGET)

# Valgrind para verificação de vazamentos de memória
valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

.PHONY: all clean run test valgrind