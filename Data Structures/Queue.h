//que.h
#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "Process.h"

struct node {
    struct Process* process;
    struct node* next;
};

struct Queue {
    struct node* front;
};

void enqueue(struct Process* process, struct Queue* queue) {
    struct node* new_node = (struct node*)malloc(sizeof(struct node));
    new_node->process = process;
    new_node->next = NULL;
    
    if (queue->front == NULL) {
        queue->front = new_node;
    } else {
        struct node* temp = queue->front;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new_node;
    }
}

struct Process* dequeue(struct Queue* queue) {
    if (queue->front == NULL) {
        return NULL;
    }
    struct Process* process = queue->front->process;
    struct node* temp = queue->front;
    queue->front = queue->front->next;
    free(temp);  
    return process;
}

struct Process* peek(struct Queue* queue) {
    if (queue->front == NULL) {
        return NULL;
    }
    return queue->front->process;
}

int isEmpty(struct Queue* queue) {
    return queue->front == NULL;
}

int size(struct Queue* queue) {
    int count = 0;
    struct node* temp = queue->front;
    while (temp != NULL) {
        count++;
        temp = temp->next;
    }
    return count;
}

void clear(struct Queue* queue) {
    while (!isEmpty(queue)) {
        struct Process* p = dequeue(queue);
        free(p); 
    }
}

void destroy(struct Queue* queue) {
    clear(queue);
    free(queue);
}

void printQueue(struct Queue* queue) {
    struct node* temp = queue->front;
    while (temp != NULL) {
        printf("Process ID: %d, Arrival: %d, Run Time: %d, Priority: %d\n",
               temp->process->id,
               temp->process->arrival_time,
               temp->process->run_time,
               temp->process->priority);
        temp = temp->next;
    }
    printf("\n");
}

void initQueue(struct Queue* queue) {
    queue->front = NULL;
}

#endif