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

// Variáveis globais do sistema
static task_t *current_task = NULL;   // Tarefa atual
static task_t main_task;              // Tarefa principal
static task_t dispatcher_task;        // Tarefa dispatcher
static int task_counter = 0;          // Contador de IDs
static queue_t *ready_queue = NULL;   // Fila de tarefas prontas
static task_t *sleeping_queue = NULL; // Fila de tarefas adormecidas
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

  // Se existe tarefa atual
  if (current_task)
  {
    // Incrementa o tempo de processador da tarefa atual
    // Não diferencia entre tarefas de usuário e sistema
    current_task->processor_time++;

    // Decrementa o quantum apenas para tarefas de usuário
    if (current_task->task_type == USER_TASK)
    {
      task_quantum--;

      // Se o quantum chegou a zero, preempta a tarefa
      if (task_quantum == 0)
      {
        // Marca tarefa como pronta e devolve o controle ao dispatcher
        if (current_task->status == TASK_RUNNING)
        {
          task_yield();
        }
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

// Função para verificar e acordar tarefas adormecidas
void check_sleeping_tasks()
{
  if (sleeping_queue == NULL)
    return;

  unsigned int current_time = systime();
  task_t *current = (task_t *)sleeping_queue;
  task_t *first = current;
  task_t *next;

  // Percorre a fila de tarefas adormecidas
  do
  {
    next = current->next;

    // Se chegou a hora de acordar esta tarefa
    if (current_time >= current->wake_time)
    {
      // Acorda a tarefa (remove da fila de sleep e coloca na fila de prontas)
      task_awake(current, &sleeping_queue);
    }

    current = next;

    // Se a fila ficou vazia, para o loop
    if (sleeping_queue == NULL)
      break;

  } while (current != first && sleeping_queue != NULL);
}

// Corpo do dispatcher
void dispatcher_body(void *arg)
{
  task_t *next;

  // Salva o momento de início do dispatcher
  dispatcher_task.start_time = systime();

  // Enquanto houverem tarefas de usuário ou tarefas adormecidas
  while (user_tasks_count > 0 || sleeping_queue != NULL)
  {
    // Verifica se há tarefas adormecidas que devem acordar
    check_sleeping_tasks();

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
      // Se status == TASK_SUSPENDED, não faz nada (fica suspensa)
    }
  }

  // Calcula o tempo de execução do dispatcher
  dispatcher_task.execution_time = systime() - dispatcher_task.start_time;

  // Imprime as estatísticas do dispatcher antes de encerrar
  printf("Task %d exit: execution time %6d ms, processor time %6d ms, %d activations\n",
         dispatcher_task.id, dispatcher_task.execution_time, dispatcher_task.processor_time,
         dispatcher_task.activations);

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

// Faz com que a tarefa atual fique suspensa durante o intervalo indicado em milissegundos
void task_sleep(int time_sleep)
{
  // Se o tempo é zero ou negativo, não faz nada
  if (time_sleep <= 0)
    return;

  // Calcula o momento em que a tarefa deve acordar
  current_task->wake_time = systime() + time_sleep;

  // Suspende a tarefa atual na fila de tarefas adormecidas
  task_suspend(&sleeping_queue);
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
  main_task.waiting_queue = NULL;  // Inicializa a fila de espera
  main_task.wake_time = 0;         // Inicializa o campo wake_time

  // Inicializa os contadores de tempo
  main_task.execution_time = 0;
  main_task.processor_time = 0;
  main_task.activations = 0;
  main_task.start_time = 0;
  main_task.last_activation = 0;

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

  // Registra o início da execução da main
  main_task.start_time = systime();
  main_task.last_activation = systime();
  main_task.activations = 1;
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
  task->context.uc_link = &dispatcher_task.context; // Quando terminar, volta para o dispatcher

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
  task->waiting_queue = NULL;  // Inicializa a fila de espera
  task->wake_time = 0;         // Inicializa o campo wake_time

  // Inicializa os contadores de tempo
  task->execution_time = 0;
  task->processor_time = 0;
  task->activations = 0;
  task->start_time = systime();
  task->last_activation = 0;

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

  // Incrementa o contador de ativações
  task->activations++;

  // Salva o tempo da última ativação
  task->last_activation = systime();

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

  // Calcula o tempo total de execução da tarefa
  current_task->execution_time = systime() - current_task->start_time;

  // Acorda todas as tarefas que estavam esperando por esta tarefa
  while (current_task->waiting_queue != NULL)
  {
    task_t *waiting_task = current_task->waiting_queue;
    task_awake(waiting_task, &(current_task->waiting_queue));
  }

  // Imprime as estatísticas da tarefa
  printf("Task %d exit: running time %6d ms, cpu time %6d ms, %d activations\n",
         current_task->id, current_task->execution_time, current_task->processor_time,
         current_task->activations);

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

// Libera a CPU
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

// Suspende a tarefa atual
void task_suspend(task_t **queue)
{
  // Se a tarefa atual está na fila de prontas, remove dela
  if (current_task->status == TASK_READY)
  {
    queue_remove(&ready_queue, (queue_t *)current_task);
  }

  // Ajusta o status da tarefa atual para suspensa
  current_task->status = TASK_SUSPENDED;

  // Se a fila não é nula, insere a tarefa atual nela
  if (queue != NULL)
  {
    queue_append((queue_t **)queue, (queue_t *)current_task);
  }

  // Retorna ao dispatcher
  task_switch(&dispatcher_task);
}

// Acorda uma tarefa que está suspensa em uma dada fila
void task_awake(task_t *task, task_t **queue)
{
  if (task == NULL)
    return;

  // Se a fila não é nula, retira a tarefa dessa fila
  if (queue != NULL && *queue != NULL)
  {
    queue_remove((queue_t **)queue, (queue_t *)task);
  }

  // Ajusta o status da tarefa para pronta
  task->status = TASK_READY;

  // Insere a tarefa na fila de tarefas prontas
  queue_append(&ready_queue, (queue_t *)task);

  // Continua a tarefa atual (não retorna ao dispatcher)
}

// A tarefa corrente aguarda o encerramento de outra task
int task_wait(task_t *task)
{
  // Verifica se a tarefa existe
  if (task == NULL)
    return -1;

  // Se a tarefa já terminou, retorna o código de saída imediatamente
  if (task->status == TASK_TERMINATED)
  {
    int exit_code = task->exit_code;
    return exit_code;
  }

  // Marca a tarefa atual como suspensa
  current_task->status = TASK_SUSPENDED;

  // Adiciona a tarefa atual na fila de espera da tarefa especificada
  queue_append((queue_t **)&(task->waiting_queue), (queue_t *)current_task);

  // Retorna ao dispatcher
  task_switch(&dispatcher_task);

  // Quando a tarefa atual for acordada, retorna o código de saída da tarefa esperada
  return task->exit_code;
}