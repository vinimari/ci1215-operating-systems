#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>

// Estados das tarefas
#define TASK_READY 0
#define TASK_RUNNING 1
#define TASK_SUSPENDED 2
#define TASK_TERMINATED 3

// Estrutura do TCB (Task Control Block)
typedef struct task_t
{
  struct task_t *prev, *next; // Ponteiros para filas
  int id;                     // ID da tarefa
  ucontext_t context;         // Contexto de execução
  short status;               // Estado atual
  void *stack;                // Pilha da tarefa
  int exit_code;              // Código de saída
} task_t;

// Estruturas para sincronização (vazias por enquanto)
typedef struct
{
  // A ser implementado
} semaphore_t;

typedef struct
{
  // A ser implementado
} mutex_t;

typedef struct
{
  // A ser implementado
} barrier_t;

typedef struct
{
  // A ser implementado
} mqueue_t;

#endif