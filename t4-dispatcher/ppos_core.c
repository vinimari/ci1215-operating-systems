#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include "ppos.h"
#include "ppos_data.h"
#include "queue.h"

#define STACKSIZE 64 * 1024 // 64KB por tarefa

// Variáveis globais do sistema
static task_t *current_task = NULL; // Tarefa atual
static task_t main_task;            // Tarefa principal
static task_t dispatcher_task;      // Tarefa dispatcher
static int task_counter = 0;        // Contador de IDs
static queue_t *ready_queue = NULL; // Fila de tarefas prontas
static int user_tasks_count = 0;    // Contador de tarefas de usuário

// Escalonador FCFS
task_t *scheduler()
{
  return (ready_queue != NULL) ? (task_t *)ready_queue : NULL;
}

// Corpo do dispatcher
void dispatcher_body(void *arg)
{
  (void)arg;

  // Remove a si mesmo da fila de prontos
  queue_remove(&ready_queue, (queue_t *)&dispatcher_task);

  while (user_tasks_count > 0)
  {
    task_t *next = scheduler();

    if (next != NULL)
    {
      printf("Dispatcher: switching to task %d\n", next->id); // Debug
      task_switch(next);

      // Trata estado da tarefa após retorno
      switch (next->status)
      {
      case TASK_TERMINATED:
        queue_remove(&ready_queue, (queue_t *)next);
        user_tasks_count--;
        free(next->stack);
        break;

      case TASK_SUSPENDED:
        queue_remove(&ready_queue, (queue_t *)next);
        break;

      default:
        break;
      }
    }
  }

  // Todas tarefas finalizadas - volta para main
  task_switch(&main_task);
}

// Inicializa o sistema
void ppos_init()
{
  setvbuf(stdout, 0, _IONBF, 0);

  // Inicializa a tarefa main
  main_task.id = 0;
  main_task.status = TASK_READY;
  main_task.prev = main_task.next = NULL;
  main_task.stack = NULL;
  main_task.exit_code = 0;

  if (getcontext(&main_task.context) == -1)
  {
    perror("ppos_init: getcontext error");
    exit(1);
  }

  ready_queue = NULL;
  user_tasks_count = 0;

  // Cria o dispatcher
  if (task_init(&dispatcher_task, dispatcher_body, NULL) < 0)
  {
    fprintf(stderr, "Failed to create dispatcher task\n");
    exit(1);
  }

  current_task = &main_task;

  // Imediatamente transfere controle para o dispatcher
  task_switch(&dispatcher_task);
}

// Cria uma nova tarefa
int task_init(task_t *task, void (*start_routine)(void *), void *arg)
{
  if (!task)
    return -1;

  // Aloca a pilha
  task->stack = malloc(STACKSIZE);
  if (!task->stack)
  {
    perror("task_init: stack allocation error");
    return -1;
  }

  // Obtém o contexto
  if (getcontext(&task->context) == -1)
  {
    free(task->stack);
    perror("task_init: getcontext error");
    return -1;
  }

  // Configura o contexto
  task->context.uc_stack.ss_sp = task->stack;
  task->context.uc_stack.ss_size = STACKSIZE;
  task->context.uc_stack.ss_flags = 0;
  task->context.uc_link = 0;

  // Cria o contexto com a função de entrada
  makecontext(&task->context, (void (*)(void))start_routine, 1, arg);

  // Configura os demais campos
  task->id = task_counter++;
  task->status = TASK_READY;
  task->prev = task->next = NULL;
  task->exit_code = 0;

  // Adiciona à fila de prontos
  queue_append((queue_t **)&ready_queue, (queue_t *)task);
  user_tasks_count++;
  printf("Task %d created successfully\n", task->id); // Debug
  return task->id;
}

// Troca para outra tarefa
int task_switch(task_t *task)
{
  if (!task)
    return -1;

  task_t *old = current_task;
  current_task = task;

  old->status = TASK_READY;
  task->status = TASK_RUNNING;

  if (swapcontext(&old->context, &task->context) == -1)
  {
    perror("task_switch: swapcontext error");
    return -1;
  }

  return 0;
}

// Finaliza a tarefa atual
void task_exit(int exit_code)
{
  if (current_task == &main_task)
  {
    // Main task tentando sair - transfere para dispatcher primeiro
    current_task->status = TASK_TERMINATED;
    current_task->exit_code = exit_code;
    task_switch(&dispatcher_task);

    // Se o dispatcher retornar, então finaliza o programa
    exit(exit_code);
  }
  else
  {
    // Outras tarefas voltam para o dispatcher
    current_task->status = TASK_TERMINATED;
    current_task->exit_code = exit_code;
    task_switch(&dispatcher_task);
  }
}

// Libera a CPU voluntariamente
void task_yield()
{
  if (current_task == &main_task || current_task == &dispatcher_task)
  {
    return;
  }

  // Reinsere no final da fila
  queue_remove((queue_t **)&ready_queue, (queue_t *)current_task);
  queue_append((queue_t **)&ready_queue, (queue_t *)current_task);

  current_task->status = TASK_READY;
  task_switch(&dispatcher_task);
}

// Retorna o ID da tarefa atual
int task_id()
{
  return current_task ? current_task->id : 0;
}