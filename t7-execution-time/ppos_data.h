#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>

// Estados das tarefas
#define TASK_READY 0
#define TASK_RUNNING 1
#define TASK_SUSPENDED 2
#define TASK_TERMINATED 3

// Tipos de tarefas
#define SYSTEM_TASK 0 // Tarefa de sistema (não preemptável)
#define USER_TASK 1   // Tarefa de usuário (preemptável)

// Estrutura do TCB (Task Control Block)
typedef struct task_t
{
  struct task_t *prev, *next; // Ponteiros para filas
  int id;                     // ID da tarefa
  ucontext_t context;         // Contexto de execução
  short status;               // Estado atual
  void *stack;                // Pilha da tarefa
  int exit_code;              // Código de saída
  int static_prio;            // Prioridade estática (-20 a +20)
  int dynamic_prio;           // Prioridade dinâmica (para envelhecimento)
  int task_type;              // Tipo da tarefa (SYSTEM_TASK ou USER_TASK)
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