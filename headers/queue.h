#ifndef QUEUE_H
#define QUEUE_H

#include <sys/types.h>

#define MAX_QUEUE_SIZE 16

typedef struct queue_entry {
	struct queue_entry 	*next;
	struct queue_entry 	*prev;
	void				*data;
} queue_entry;

typedef struct queue {
	struct queue_entry *head;
	struct queue_entry *tail;
	size_t              length;
} queue;

queue *queue_create();
// static queue_entry *queue_entry_new(void *data);
ssize_t queue_length(queue *q);

int queue_is_empty(queue *q);
int queue_is_full(queue *q);

int queue_head_push(queue *q, void *data);
int queue_tail_push(queue *q, void *data);

void queue_head_pop(queue *q);
void queue_tail_pop(queue *q);

void *queue_head_peek(queue *q);
void *queue_tail_peek(queue *q);

void queue_entry_destroy(queue *q, queue_entry *entry);
void queue_flush(queue * q);

#endif