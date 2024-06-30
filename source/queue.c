/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#include <stdlib.h>
#include <string.h>

#include "queue.h"

void init_inner(Queue *q) {
  q->head = NULL;
  q->tail = NULL;
}

int is_inner_empty(Queue *q) { return q->head == NULL; }

void enqueue_inner(Queue *q, char *data) {
  QueueNode *new_node = (QueueNode *)malloc(sizeof(QueueNode));
  strcpy(new_node->data, data);
  new_node->next = NULL;

  if (is_inner_empty(q)) {
    q->head = new_node;
    q->tail = new_node;
  } else {
    q->tail->next = new_node;
    q->tail = new_node;
  }
}

void dequeue_inner(Queue *q, char *dest) {
  struct Node *node = q->head;
  strcpy(dest, node->data);

  q->head = q->head->next;

  if (q->head == NULL) {
    q->tail = NULL;
  }

  free(node);
}

void init(PrioQueue *q, int max_size) {
  init_inner(&q->prio0);
  init_inner(&q->prio1);
  q->size = 0;
  q->max_size = max_size;
}

int is_empty(PrioQueue *q) { return q->size == 0; }

int is_full(PrioQueue *q) { return q->size == q->max_size; }

void enqueue(PrioQueue *q, char *data, int prio) {
  if (prio == 0) {
    enqueue_inner(&q->prio0, data);
  } else {
    enqueue_inner(&q->prio1, data);
  }
  q->size++;
}

void dequeue(PrioQueue *q, char *dest, int *prio) {
  if (!is_inner_empty(&q->prio1)) {
    dequeue_inner(&q->prio1, dest);
    *prio = 1;
  } else {
    dequeue_inner(&q->prio0, dest);
    *prio = 0;
  }

  q->size--;
}
