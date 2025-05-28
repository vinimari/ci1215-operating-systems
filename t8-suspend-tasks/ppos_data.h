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

  // Campos para contabilização de uso
  unsigned int execution_time;  // Tempo total de execução (em ms)
  unsigned int processor_time;  // Tempo de processador (em ms)
  unsigned int activations;     // Número de ativações
  unsigned int start_time;      // Momento de início da tarefa
  unsigned int last_activation; // Momento da última ativação

  // Campo para sincronização
  struct task_t *waiting_queue; // Fila de tarefas esperando por esta tarefa
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