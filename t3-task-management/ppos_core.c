#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include "ppos.h"
#include "ppos_data.h"

#define STACKSIZE 64 * 1024 // 64KB stack size for each task

// Global system variables
static task_t *current_task = NULL; // Currently executing task
static task_t main_task;            // Main task descriptor
static int task_counter = 0;        // Counter for generating unique IDs

// Initializes the operating system
void ppos_init()
{
  // Disables stdout buffering to avoid context switch issues
  setvbuf(stdout, 0, _IONBF, 0);

  // Initializes the main task
  main_task.id = 0;
  main_task.status = 0; // 0 means "ready/running"
  main_task.prev = NULL;
  main_task.next = NULL;

  // Gets the current context and saves it in the main task
  if (getcontext(&main_task.context) == -1)
  {
    perror("ppos_init: getcontext error");
    exit(1);
  }

  // Sets the current task as main
  current_task = &main_task;
  task_counter = 1; // Next task will have ID 1
}

// Starts a new task
int task_init(task_t *task, void (*start_routine)(void *), void *arg)
{
  if (!task)
    return -1;

  // Configures the new task's context
  if (getcontext(&task->context) == -1)
  {
    perror("task_init: getcontext error");
    return -2;
  }

  // Allocates stack for the task
  char *stack = malloc(STACKSIZE);
  if (!stack)
  {
    perror("task_init: stack allocation error");
    return -3;
  }

  // Sets up the context
  task->context.uc_stack.ss_sp = stack;
  task->context.uc_stack.ss_size = STACKSIZE;
  task->context.uc_stack.ss_flags = 0;
  task->context.uc_link = 0;

  // Creates the context with the function to be executed
  makecontext(&task->context, (void (*)(void))start_routine, 1, arg);

  // Configures other task fields
  task->id = task_counter++;
  task->status = 0; // 0 means "ready"
  task->prev = NULL;
  task->next = NULL;

  return task->id;
}

// Transfers processor to another task
int task_switch(task_t *task)
{
  if (!task)
    return -1;

  task_t *old_task = current_task;
  current_task = task;

  // Saves current context and activates the new context
  if (swapcontext(&old_task->context, &task->context) == -1)
  {
    perror("task_switch: swapcontext error");
    return -2;
  }

  return 0;
}

// Terminates the current task
void task_exit(int exit_code)
{
  // Returns to the main task
  task_switch(&main_task);
}

// Returns the current task's ID
int task_id()
{
  return current_task ? current_task->id : 0;
}