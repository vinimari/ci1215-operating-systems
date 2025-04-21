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

// Escalonador FCFS - seleciona a primeira tarefa da fila de prontas
task_t *scheduler()
{
  if (ready_queue == NULL)
    return NULL;

  // Retorna a primeira tarefa da fila (FCFS)
  return (task_t *)ready_queue;
}

// Corpo do dispatcher
void dispatcher_body(void *arg)
{
  task_t *next;

  // Enquanto houverem tarefas de usuário
  while (user_tasks_count > 0)
  {
    // Escolhe a próxima tarefa a executar
    next = scheduler();

    if (next != NULL)
    {
      // Remove da fila de prontas
      queue_remove(&ready_queue, (queue_t *)next);

      // Transfere controle para a próxima tarefa
      task_switch(next);

      // Voltando ao dispatcher, trata a tarefa de acordo com seu estado
      if (next->status == TASK_TERMINATED)
      {
        // Decrementa o contador de tarefas de usuário
        user_tasks_count--;

        // Libera a pilha alocada para a tarefa
        if (next->stack)
        {
          free(next->stack);
          next->stack = NULL;
        }
      }
      else if (next->status == TASK_READY)
      {
        // Reinsere na fila de prontas se não terminou
        queue_append(&ready_queue, (queue_t *)next);
      }
    }
  }

  // Encerra a tarefa dispatcher retornando à main
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

  // Define a tarefa atual como a main
  current_task = &main_task;

  // Inicializa o contador de tarefas de usuário
  user_tasks_count = 0;

  // Cria a tarefa dispatcher
  task_init(&dispatcher_task, dispatcher_body, NULL);

  // A tarefa dispatcher não deve ser contada como tarefa de usuário
  queue_remove(&ready_queue, (queue_t *)&dispatcher_task);
  user_tasks_count--;
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
  task->context.uc_link = 0; // Quando terminar, encerra (não volta para lugar nenhum)

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

  return task->id;
}

// Troca para outra tarefa
int task_switch(task_t *task)
{
  if (!task)
    return -1;

  task_t *old = current_task;
  current_task = task;

  // Atualiza o estado da tarefa para executando
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
  current_task->exit_code = exit_code;

  if (current_task == &main_task)
  {
    // Se main está saindo e ainda há tarefas de usuário, vai para o dispatcher
    if (user_tasks_count > 0)
    {
      task_switch(&dispatcher_task);
    }
    // Se não houver mais tarefas, termina o programa
    exit(exit_code);
  }
  else
  {
    // Marca como terminada
    current_task->status = TASK_TERMINATED;

    // Volta para o dispatcher
    task_switch(&dispatcher_task);
  }
}

// Libera a CPU voluntariamente
void task_yield()
{
  // Marca tarefa como pronta e devolve o controle ao dispatcher
  current_task->status = TASK_READY;
  task_switch(&dispatcher_task);
}

// Retorna o ID da tarefa atual
int task_id()
{
  return current_task ? current_task->id : 0;
}