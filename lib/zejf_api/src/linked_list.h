#ifndef _ZEJF_QUEUE_H
#define _ZEJF_QUEUE_H

#include <stddef.h>
#include <stdbool.h>

typedef struct node_t Node;

struct node_t{
    Node* next;
    Node* previous;
    void* item;
};

typedef struct linked_list_t{
    size_t capacity;
    size_t item_count;
    Node* head;
    Node* tail;
} LinkedList;

LinkedList* list_create(size_t capacity);

void list_destroy(LinkedList* list, void* (*destructor)(void*));

bool list_push(LinkedList* list, void* item);

Node* list_peek(LinkedList* list);

void* list_pop(LinkedList* list);

void* list_remove(LinkedList* list, Node* node);

void list_prioritise(LinkedList* list, Node* node);

bool list_is_full(LinkedList* list);

bool list_is_empty(LinkedList* list);

#endif