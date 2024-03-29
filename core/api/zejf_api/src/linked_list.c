#include "linked_list.h"

#include <stdio.h>
#include <stdlib.h>

LinkedList *list_create(size_t capacity) {
    LinkedList *list = malloc(sizeof(LinkedList));
    if (list == NULL) {
        return NULL;
    }

    list->capacity = capacity;
    list->head = NULL;
    list->tail = NULL;
    list->item_count = 0;

    return list;
}

void list_destroy(LinkedList *list, void *(*destructor)(void *) ) {
    Node *node = list->tail;
    if (node == NULL) {
        goto end;
    }

    do {
        destructor(node->item);
        Node *tmp = node;
        node = node->next;
        free(tmp);
    } while (node != list->tail);

end:
    free(list);
}

bool list_push(LinkedList *list, void *item) {
    if (list == NULL) {
        return false;
    }

    if (list_is_full(list)) {
        return false;
    }

    Node *new_node = malloc(sizeof(Node));
    if (new_node == NULL) {
        return false;
    }
    new_node->item = item;

    if (list->head == NULL) {
        new_node->next = new_node;
        new_node->previous = new_node;
        list->head = new_node;
        list->tail = new_node;
    } else {
        list->head->next = new_node;
        new_node->previous = list->head;
        list->head = new_node;
        new_node->next = list->tail;
        list->tail->previous = new_node;
    }

    list->item_count++;

    return true;
}

Node *list_peek(LinkedList *list) {
    if (list == NULL) {
        return NULL;
    }

    return list->tail;
}

void *list_remove(LinkedList *list, Node *node) {
    if (list == NULL) {
        return NULL;
    }

    if (node == NULL) {
        return NULL;
    }

    if (node == list->tail && node == list->head) {
        list->tail = NULL;
        list->head = NULL;
    } else if (node == list->tail) {
        list->tail = node->next;
    } else if (node == list->head) {
        list->head = node->previous;
    }

    node->previous->next = node->next;
    node->next->previous = node->previous;
    list->item_count--;

    void *result = node->item;

    free(node);

    return result;
}

void *list_pop(LinkedList *list) {
    return list_remove(list, list->tail);
}

void list_prioritise(LinkedList *list, Node *node) {
    list->tail = node;
    list->head = node->previous;
}

inline bool list_is_full(LinkedList *list) {
    return list->item_count == list->capacity;
}

inline bool list_is_empty(LinkedList *list) {
    return list->item_count == 0;
}
