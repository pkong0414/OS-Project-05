//queue.c

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "queue.h"
//#include "sharedHandler.h"

Queue* initQueue(){
    //creating a new queue
    Queue *newQueue;
    newQueue = (Queue *) malloc( sizeof(Queue) );
    newQueue->front = 0;
    newQueue->rear = 0;
    newQueue->currentCapacity = 0;
    newQueue->size = SIZE;
    //creating an array of 18 elements
    newQueue->arr = (unsigned int*) malloc(SIZE * sizeof(unsigned int));
    int i;
    for( i = 0; i < SIZE; i++){
        newQueue->arr[i] = -1;
    }
    return newQueue;
}

int push( Queue* queue, int localPID ){
    //this is a FIFO. We'll just put the things towards the rear.
    if( queue->currentCapacity == ( queue->size ) ) {
        printf("the queue is full!\n");
        return -1;
    } else {
        queue->arr[ queue->rear ] = localPID;
        queue->rear = (queue->rear + 1) % queue->size;
        queue->currentCapacity++;
        return 0;
    }
}

int pop( Queue *queue ){
    //popping the front of the queue.
    unsigned int localpid;
    if( isQueueEmpty( queue ) == true ) {
        printf("queue is empty\n");
        return -1;
    } else {
        //the case of a non empty queue
        localpid = queue->arr[queue->front];
        //queue is getting reset to 0 before we set the front where it needs to be in the queue
        queue->arr[queue->front] = -1;
        queue->front = (queue->front + 1) % queue->size;
        queue->currentCapacity--;
        return localpid;
    }
}

void removeQueue( Queue *queue){
    int i;
    for(i = 0; i < queue->size; i++){
        queue->arr[i] = -1;
    }
    queue->currentCapacity = 0;
    queue->front = 0;
    queue->rear = 0;
}

int isQueueEmpty( Queue* queue ){
    int i;
    for( i = 0; i < queue->size; i++ ){
        if( queue->arr[i] > -1 )
            return false;
    }
    return true;
}

void printQueue( Queue* queue ) {
    int i;
    int j;
//    if( isQueueEmpty(queue) ){
//        printf("there is no queue\n");
//        return;
//    }

    for ( i = 0; i < queue->size; i++) {
        j = (queue->front + i) % queue->size;
        if( queue->arr[j] == -1 ){
            continue;
        }
        printf("index %d: %d\n", j, (unsigned int)queue->arr[j]);
    }
}

int findFrontPID( Queue *queue ){
    if( queue->currentCapacity == 0 ){
        //this means our queue is empty
        return -1;
    } else {
        return queue->arr[queue->front];
    }
}

int rotateQueue(Queue *queue){
    //we'll use this to rotate around the queue to handle blocked queue and pop the ones handled.

    // currentCapacity will allow us to wrap around the queue based on the size of the queue vs where's the front.
    // This do while ensures that we will be rotating the queue in the right way. If there's a -1 we know that it is
    // an empty slot
    do{
        queue->front = (queue->front+1) % queue->size;
        queue->rear = (queue->rear+1) % queue->size;
    }while( queue->arr[queue->front] == -1);
    return queue->front;
}