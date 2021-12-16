#include "../headers/queue.h"
#include "../headers/utils.h"

#include <stdlib.h>
#include <stdio.h>

static queue_entry *queue_entry_new(void *data)
{
	struct queue_entry * entry = allocate_memory(sizeof(queue_entry));

	if (!entry)
		return NULL;

	entry -> data = data;
	entry -> next = NULL;
	entry -> prev = NULL;

	return entry;
}

queue *queue_create()
{
	queue *q = allocate_memory(sizeof(queue));
	if (q == NULL)
		return NULL;

	q -> head = NULL;
	q -> tail = NULL;
	q -> length = 0;

	return q;
}

ssize_t queue_length(queue *q)
{
	if (q == NULL) {
		fprintf(stderr, "queue is null");
		return 0;
	}

	return q -> length;
}

int queue_is_empty(queue *q)
{
	if (q == NULL) {
		fprintf(stderr, "queue is null");
		return -1;
	}

	return q -> length == 0;
}

int queue_is_full(queue *q)
{
	if (q == NULL) {
		fprintf(stderr, "queue is null");
		return -1;
	}

	return (queue_length(q) == MAX_QUEUE_SIZE);
}

void queue_entry_destroy(queue *q, queue_entry *entry)
{
	if (entry == NULL) return;
	if (entry == q->head) return queue_head_pop(q);
	if (entry == q->tail) return queue_tail_pop(q);

	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;

	q->length--;
	free(entry);
}

int queue_head_push(queue *q, void *data)
{
	struct queue_entry *entry;

	if (q == NULL || queue_is_full(q)) {
        fprintf(stderr, "pushing to an invalid queue");
		return -1;
	}

	entry = queue_entry_new(data);
	if (entry == NULL)
		return -1;

	if (q -> head == NULL) 
    {
		q->head = entry;
		q->tail = entry;
	} 

    else 
    {
		q->head->prev = entry;
		q->head = entry;
	}

	q -> length++;

	return 0;
}

void queue_head_pop(queue *q)
{
	queue_entry *entry;

	if (q == NULL || queue_is_empty(q)) {
        fprintf(stderr, "popping from an invalid queue");
		return;
	}

	entry = q -> head;
	q -> head = entry -> next;

	if (q -> head == NULL) q -> tail = NULL;
    else q -> head -> prev = NULL;

	q->length--;
	free(entry);
}

int queue_tail_push(queue *q, void * data)
{
	struct queue_entry * entry;

	if (q == NULL || queue_is_full(q)) {
        fprintf(stderr, "pushing to an invalid queue");
		return -1;
	}

	entry = queue_entry_new(data);
	if (entry == NULL)
		return -1;

	if (q -> tail == NULL)
    {
		q -> head = entry;
		q -> tail = entry;
	} 
    
    else 
    {
		q -> tail -> next = entry;
		q -> tail = entry;
	}

	q -> length++;
	return 0;
}

void queue_tail_pop(queue *q)
{
	struct queue_entry * entry;

    if (q == NULL || queue_is_empty(q)) {
        fprintf(stderr, "popping from an invalid queue");
		return;
	}

	entry = q -> tail;
	q -> tail = entry -> prev;

	if (q -> tail == NULL) q->head = NULL;
	else q -> tail -> next = NULL;

	q->length--;
	free(entry);
}

void *queue_head_peek(queue *q)
{
    if (q == NULL || queue_is_empty(q)) {
		return NULL;
	}
	return q -> head -> data;
}

void *queue_tail_peek(queue *q)
{
	if (q == NULL || queue_is_empty(q)) {
		return NULL;
	}
	return q -> head -> data;
}

void queue_flush(queue * q)
{
	// int i = 0;
    while (!queue_is_empty(q))
	{
		// printf("<routing>: free %d entry\n", ++i);
		queue_head_pop(q);
	}

	free(q);
}