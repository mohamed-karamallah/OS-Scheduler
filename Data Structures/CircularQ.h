#ifndef CIRCULARQ_H
#define CIRCULARQ_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "Process.h"

struct circular_node {
    struct Process* process;
    struct circular_node* next;
};


struct QueueQ {
    struct circular_node* front;  // Front of the queue
    struct circular_node* rear;   // Rear of the queue (for faster enqueueing)
    int size;            // Number of processes in queue
};


void initQueueQ(struct QueueQ* q) {
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
    printf("[CircularQ] Queue initialized\n");
}

bool isEmptyQ(struct QueueQ* q) {
    return (q->front == NULL);
}


int sizeQ(struct QueueQ* q) {
    return q->size;
}

void enqueueQ(struct Process* process, struct QueueQ* q) {
    if (process == NULL) {
        printf("[CircularQ] Error: Cannot enqueue NULL process\n");
        return;
    }

    struct circular_node* new_node = (struct circular_node*)malloc(sizeof(struct circular_node));
    if (!new_node) {
        perror("[CircularQ] Failed to allocate memory for queue node");
        return;
    }
    
    new_node->process = process;
    new_node->next = NULL;
    

    if (isEmptyQ(q)) {
        q->front = new_node;
        q->rear = new_node;
    } else {

        q->rear->next = new_node;
        q->rear = new_node;
    }
    

    q->size++;
    
    printf("[CircularQ] Enqueued Process %d (arrival=%d, remaining=%d), queue size now: %d\n", 
           process->id, process->arrival_time, process->remaining_time, q->size);
    

    printf("[CircularQ] Queue after enqueue: ");
    struct circular_node* current = q->front;
    while (current != NULL) {
        printf("P%d(arr=%d,rem=%d) ", 
               current->process->id, 
               current->process->arrival_time,
               current->process->remaining_time);
        current = current->next;
    }
    printf("\n");
}


struct Process* dequeueQ(struct QueueQ* q) {
    // Check if queue is empty
    if (isEmptyQ(q)) {
        printf("[CircularQ] Error: Queue is empty, cannot dequeue\n");
        return NULL;
    }
    

    struct circular_node* temp = q->front;
    struct Process* process = temp->process;
    

    if (process == NULL) {
        printf("[CircularQ] Error: Process is NULL in dequeue\n");
        

        q->front = q->front->next;
        if (q->front == NULL) {
            q->rear = NULL;
        }
        free(temp);
        q->size--;
        
        return NULL;
    }

    q->front = q->front->next;
    

    if (q->front == NULL) {
        q->rear = NULL;
    }
 
    free(temp);
    q->size--;
    
    printf("[CircularQ] Dequeued Process %d (arrival=%d, remaining=%d), queue size now: %d\n", 
           process->id, process->arrival_time, process->remaining_time, q->size);
    

    printf("[CircularQ] Queue after dequeue: ");
    struct circular_node* current = q->front;
    while (current != NULL) {
        printf("P%d(arr=%d,rem=%d) ", 
               current->process->id,
               current->process->arrival_time,
               current->process->remaining_time);
        current = current->next;
    }
    printf("\n");
    
    return process;
}


struct Process* peekQ(struct QueueQ* q) {
    if (isEmptyQ(q)) {
        printf("[CircularQ] Queue is empty, cannot peek\n");
        return NULL;
    }
    
    if (q->front->process == NULL) {
        printf("[CircularQ] Error: Front process is NULL in peek\n");
        return NULL;
    }
    
    return q->front->process;
}

void printQueueQ(struct QueueQ* q) {
    if (isEmptyQ(q)) {
        printf("[CircularQ] Queue is empty\n");
        return;
    }
    
    printf("[CircularQ] Queue contents: ");
    struct circular_node* temp = q->front;
    int count = 0;
    while (temp != NULL && count < 20) { 
        if (temp->process != NULL) {
            printf("P%d(arr=%d,rem=%d) ", 
                   temp->process->id,
                   temp->process->arrival_time,
                   temp->process->remaining_time);
        } else {
            printf("NULL ");
        }
        temp = temp->next;
        count++;
    }
    
    if (count >= 20) {
        printf("... (truncated, possibly circular reference)");
    }
    
    printf("\n");
}


void clearQueueQ(struct QueueQ* q) {
    printf("[CircularQ] Clearing queue\n");
    while (!isEmptyQ(q)) {
        struct Process* p = dequeueQ(q);
        if (p != NULL) {
            free(p);
        }
    }
}

#endif 