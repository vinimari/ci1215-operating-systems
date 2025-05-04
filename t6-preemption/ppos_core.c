#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include "ppos.h"
#include "ppos_data.h"
#include "queue.h"

#define STACKSIZE 64 * 1024 // 64KB por tarefa
#define DEFAULT_PRIO 0      // Prioridade padrão
#define ALPHA -1            // Fator de envelhecimento
#define MIN_PRIO -20        // Prioridade máxima
#define MAX_PRIO 20         // Prioridade mínima
#define QUANTUM 20          // Quantum padrão (em ticks)
#define TICK_INTERVAL 1000  // Intervalo do temporizador (em microssegundos)

// Descomentar para depuração do preemptor
// #define DEBUG

// Variáveis globais do sistema
static task_t *current_task = NULL;   // Tarefa atual
static task_t main_task;              // Tarefa principal
static task_t dispatcher_task;        // Tarefa dispatcher
static int task_counter = 0;          // Contador de IDs
static queue_t *ready_queue = NULL;   // Fila de tarefas prontas
static int user_tasks_count = 0;      // Contador de tarefas de usuário
static unsigned int system_clock = 0; // Relógio do sistema

// Estrutura para o tratador de sinal
struct sigaction action;

// Estrutura para o temporizador
struct itimerval timer;

// Variável para controlar o quantum da tarefa atual
static int task_quantum;

// Tratador de sinal para preempção
void timer_handler(int signum)
{
  // Incrementa o relógio do sistema
  system_clock++;

  // Se a tarefa atual for uma tarefa de usuário
  if (current_task && current_task->task_type == USER_TASK)
  {
    // Decrementa o quantum da tarefa
    task_quantum--;

#ifdef DEBUG
    printf("TICK: tarefa %d, quantum %d\n", current_task->id, task_quantum);
#endif

    // Se o quantum chegou a zero, preempta a tarefa
    if (task_quantum == 0)
    {
      // Marca tarefa como pronta e devolve o controle ao dispatcher
      if (current_task->status == TASK_RUNNING)
      {
#ifdef DEBUG
        printf("PREEMPCAO: tarefa %d\n", current_task->id);
#endif

        task_yield();
      }
    }
  }
}

// Define a prioridade estática de uma tarefa (ou da atual, se task==NULL)
void task_setprio(task_t *task, int prio)
{
  // Garante que a prioridade esteja no intervalo correto
  if (prio < MIN_PRIO)
    prio = MIN_PRIO;
  else if (prio > MAX_PRIO)
    prio = MAX_PRIO;

  // Se a tarefa não foi especificada, use a tarefa atual
  if (task == NULL)
    task = current_task;

  // Define a prioridade estática e reinicia a dinâmica
  task->static_prio = prio;
  task->dynamic_prio = prio;
}

// Retorna a prioridade estática de uma tarefa (ou da atual, se task==NULL)
int task_getprio(task_t *task)
{
  // Se a tarefa não foi especificada, use a tarefa atual
  if (task == NULL)
    task = current_task;

  return task->static_prio;
}

// Escalonador - seleciona a tarefa de maior prioridade (menor valor) da fila
task_t *scheduler()
{
  if (ready_queue == NULL)
    return NULL;

  // Localiza a tarefa com maior prioridade (menor valor numérico)
  task_t *highest_prio_task = NULL;
  task_t *current = (task_t *)ready_queue;
  task_t *first = current;
  int highest_prio = MAX_PRIO + 1; // Inicializa com um valor maior que qualquer prioridade possível

  // Percorre a fila em busca da tarefa com maior prioridade (menor valor numérico)
  do
  {
    // Se a prioridade dinâmica desta tarefa for maior (valor menor)
    if (current->dynamic_prio < highest_prio)
    {
      highest_prio = current->dynamic_prio;
      highest_prio_task = current;
    }
    current = current->next;
  } while (current != first);

  // Envelhece todas as tarefas que não foram escolhidas
  current = first;
  do
  {
    if (current != highest_prio_task)
    {
      // Aplica o envelhecimento (aumenta a prioridade - diminui o valor)
      current->dynamic_prio += ALPHA;

      // Garante que não ultrapasse a prioridade máxima
      if (current->dynamic_prio < MIN_PRIO)
        current->dynamic_prio = MIN_PRIO;
    }
    current = current->next;
  } while (current != first);

  // Reseta a prioridade dinâmica da tarefa escolhida para seu valor estático
  highest_prio_task->dynamic_prio = highest_prio_task->static_prio;

  return highest_prio_task;
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

      // Reseta o quantum para a próxima tarefa
      task_quantum = QUANTUM;

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

// Inicializa o sistema de tempo
void timer_init()
{
  // Registra o tratador de sinal para SIGALRM
  action.sa_handler = timer_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  if (sigaction(SIGALRM, &action, 0) < 0)
  {
    perror("Erro em sigaction: ");
    exit(1);
  }

  // Configura o temporizador para disparar a cada 1ms
  timer.it_value.tv_usec = TICK_INTERVAL;    // primeiro disparo, em microssegundos
  timer.it_value.tv_sec = 0;                 // primeiro disparo, em segundos
  timer.it_interval.tv_usec = TICK_INTERVAL; // disparos subsequentes, em microssegundos
  timer.it_interval.tv_sec = 0;              // disparos subsequentes, em segundos

  // Ativa o temporizador
  if (setitimer(ITIMER_REAL, &timer, 0) < 0)
  {
    perror("Erro em setitimer: ");
    exit(1);
  }
}

// Retorna o relógio atual (em milisegundos)
unsigned int systime()
{
  return system_clock;
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
  main_task.static_prio = DEFAULT_PRIO;
  main_task.dynamic_prio = DEFAULT_PRIO;
  main_task.task_type = USER_TASK; // Main é uma tarefa de usuário

  if (getcontext(&main_task.context) == -1)
  {
    perror("ppos_init: getcontext error");
    exit(1);
  }

  // Define a tarefa atual como a main
  current_task = &main_task;

  // Inicializa o contador de tarefas de usuário
  user_tasks_count = 0;

  // Inicializa o relógio do sistema
  system_clock = 0;

  // Define o quantum inicial
  task_quantum = QUANTUM;

  // Cria a tarefa dispatcher como tarefa de sistema
  task_init(&dispatcher_task, dispatcher_body, NULL);

  // Define o dispatcher como tarefa de sistema
  dispatcher_task.task_type = SYSTEM_TASK;

  // A tarefa dispatcher não deve ser contada como tarefa de usuário
  queue_remove(&ready_queue, (queue_t *)&dispatcher_task);
  user_tasks_count--;

  // Inicializa o sistema de tempo (preempção)
  timer_init();
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
  task->static_prio = DEFAULT_PRIO;
  task->dynamic_prio = DEFAULT_PRIO;
  task->task_type = USER_TASK; // Por padrão, cria como tarefa de usuário

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