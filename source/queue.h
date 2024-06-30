/*
  Gonçalo Monteiro  2021217127
  Tiago Almeida     2021221615
*/

#ifndef QUEUE_H
#define QUEUE_H

#include "project_consts.h"

typedef struct Node {
  char data[BUFFER_SIZE];
  struct Node *next;
} QueueNode;

typedef struct {
  QueueNode *head;
  QueueNode *tail;
} Queue;

typedef struct {
  Queue prio0;
  Queue prio1;
  int size;
  int max_size;
} PrioQueue;

void init(PrioQueue *q, int max_size);
void enqueue(PrioQueue *q, char *data, int prio);
void dequeue(PrioQueue *q, char *dest, int *prio);
int is_empty(PrioQueue *q);
int is_full(PrioQueue *q);

#endif // QUEUE_H