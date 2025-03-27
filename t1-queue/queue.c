// Nome: Márcio Vinícius de Carvalho Marinho
// GRR: 20204089

#include <stdio.h>
#include "queue.h"

#define SUCCESS 0
#define ERROR_NULL_QUEUE -1
#define ERROR_NULL_ELEMENT -2
#define ERROR_ELEMENT_IN_QUEUE -3
#define ERROR_EMPTY_QUEUE -4
#define ERROR_ELEMENT_NOT_IN_QUEUE -5

//------------------------------------------------------------------------------
// Verifica se um elemento pertence a alguma fila
// Retorno: 1 se pertence, 0 caso contrário

static inline int is_element_in_queue(queue_t *elem) {
    return (elem->next != NULL || elem->prev != NULL);
}

//------------------------------------------------------------------------------
// Verifica se um elemento específico pertence a fila indicada
// Retorno: 1 se pertence, 0 caso contrário

static int belongs_to_queue(queue_t *queue, queue_t *elem) {
    if (queue == NULL || elem == NULL) {
        return 0;
    }
    
    queue_t *current = queue;
    do {
        if (current == elem) {
            return 1;
        }
        current = current->next;
    } while (current != queue);
    
    return 0;
}

int queue_size(queue_t *queue) {
    if (queue == NULL) {
        return 0;
    }
    
    int count = 1;
    queue_t *current = queue;
    
    while (current->next != queue) {
        count++;
        current = current->next;
    }
    
    return count;
}

void queue_print(char *name, queue_t *queue, void print_elem(void *)) {
    const char *queue_name = (name != NULL) ? name : "QUEUE";
    printf("%s: [", queue_name);
    
    if (queue == NULL) {
        printf("]\n");
        return;
    }
    
    // Imprime cada elemento da fila
    queue_t *current = queue;
    do {
        print_elem((void*)current);
        current = current->next;
        
        // Adiciona separador entre elementos
        if (current != queue) {
            printf(" ");
        }
    } while (current != queue);
    
    printf("]\n");
}

int queue_append(queue_t **queue, queue_t *elem) {
    // Validações iniciais
    if (queue == NULL) {
        fprintf(stderr, "ERRO: fila não existe\n");
        return ERROR_NULL_QUEUE;
    }
    
    if (elem == NULL) {
        fprintf(stderr, "ERRO: elemento não existe\n");
        return ERROR_NULL_ELEMENT;
    }
    
    if (is_element_in_queue(elem)) {
        fprintf(stderr, "ERRO: elemento já pertence a uma fila\n");
        return ERROR_ELEMENT_IN_QUEUE;
    }
    
    // Caso especial: fila vazia
    if (*queue == NULL) {
        *queue = elem;
        elem->next = elem;
        elem->prev = elem;
        return SUCCESS;
    }
    
    // Inserção no final da fila circular
    queue_t *first = *queue;
    queue_t *last = first->prev;
    
    elem->next = first;
    elem->prev = last;
    
    last->next = elem;
    first->prev = elem;
    
    return SUCCESS;
}

int queue_remove(queue_t **queue, queue_t *elem) {
    // Validações iniciais
    if (queue == NULL) {
        fprintf(stderr, "ERRO: fila não existe\n");
        return ERROR_NULL_QUEUE;
    }
    
    if (*queue == NULL) {
        fprintf(stderr, "ERRO: fila vazia\n");
        return ERROR_EMPTY_QUEUE;
    }
    
    if (elem == NULL) {
        fprintf(stderr, "ERRO: elemento não existe\n");
        return ERROR_NULL_ELEMENT;
    }
    
    // Verifica se o elemento pertence a fila
    if (!belongs_to_queue(*queue, elem)) {
        fprintf(stderr, "ERRO: elemento não pertence a fila indicada\n");
        return ERROR_ELEMENT_NOT_IN_QUEUE;
    }
    
    // Caso especial: elemento é o único na fila
    if (elem->next == elem) {
        *queue = NULL;
    } else {
        // Caso especial: elemento é o primeiro da fila
        if (elem == *queue) {
            *queue = elem->next;
        }
        
        // Remove o elemento da fila ajustando os ponteiros
        elem->prev->next = elem->next;
        elem->next->prev = elem->prev;
    }
    
    // Limpa os ponteiros do elemento removido
    elem->next = NULL;
    elem->prev = NULL;
    
    return SUCCESS;
}