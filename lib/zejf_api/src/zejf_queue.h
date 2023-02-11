#ifndef _ZEJF_QUEUE_H
#define _ZEJF_QUEUE_H

#include <stddef.h>
#include <stdbool.h>

typedef struct node_t Node;

struct node_t{
    Node* next;
    void* item;
};

typedef struct zejf_queue_t{
    size_t capacity;
    size_t item_count;
    Node* first;
    Node* last;
} Queue;

Queue* queue_create(size_t capacity);

void queue_destroy(Queue* queue, void* (*destructor)(void*));

bool queue_push(Queue* queue, void* item);

void* queue_peek(Queue* queue);

void* queue_pop(Queue* queue);

#endif