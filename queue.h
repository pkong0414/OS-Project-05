//queue.h

#ifndef QUEUE_H
#define QUEUE_H

#define SIZE 18

//queue functions
typedef struct QUEUE {
    unsigned int front;
    unsigned int rear;
    unsigned int currentCapacity;
    unsigned int size;
    unsigned int *arr;
} Queue;

Queue *initQueue();
int push( Queue* queue, int localpid );
int pop( Queue *queue );
void removeQueue( Queue *queue );
int isQueueEmpty( Queue *queue  );
void printQueue( Queue *queue  );
int findFrontPID( Queue *queue );

#endif