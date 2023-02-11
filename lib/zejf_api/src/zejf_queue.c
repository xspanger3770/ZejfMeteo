#include "zejf_queue.h"

#include <stdlib.h>

Queue* queue_create(size_t capacity){
    Queue* queue = malloc(sizeof(Queue));
    if(queue == NULL){
        return NULL;
    }

    queue->capacity = capacity;
    queue->first = NULL;
    queue->last = NULL;
    queue->item_count = 0;

    return queue;
}

void queue_destroy(Queue* queue, void* (*destructor)(void*)){
    Node* node = queue->first;
    while(node != NULL) {
        destructor(node->item);
        Node* tmp = node;
        node = node->next;
        free(tmp);
    }

    free(queue);
}

bool queue_push(Queue* queue, void* item){
    if(queue == NULL){
        return false;
    }

    if(queue->item_count == queue->capacity){
        return false;
    }

    Node* new_node = malloc(sizeof(Node));
    if(new_node == NULL){
        return false;
    }

    new_node->next = NULL;
    new_node->item = item;

    if(queue->last == NULL){
        queue->last = new_node;
        queue->first = new_node;
    } else {
        queue->last->next = new_node;
        queue->last = new_node;
    }

    queue->item_count++;

    return true;
}

void* queue_peek(Queue* queue){
    if(queue == NULL){
        return NULL;
    }

    if(queue->first == NULL){
        return NULL;
    }

    return queue->first->item;
}

void* queue_pop(Queue* queue){
    if(queue == NULL){
        return false;
    }

    if(queue->first == NULL){
        return NULL;
    }

    void* result = queue->first->item;
    Node* next = queue->first->next;

    free(queue->first);

    queue->first = next;
    if(next == NULL){
        queue->last = NULL;
    }

    queue->item_count--;

    return result;
}