//user_proc

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sharedHandler.h"
#include "signalHandler.h"

sharedMem *ushm;
PCB *pcb;
Message message;

void userInit( int argc, char** argv );
void request();
void release();
void releaseAllResources();
void terminate();

// boolean globals
bool terminatable = false;
bool haveResources = false;

int main(int argc, char ** argv){
    if( setupUserInterrupt() == -1 ){
        perror( "failed to set up a user kill signal.\n");
        return 1;
    }

    userInit( argc, argv );

    /*  TO DO:
     *
     *      1. We NEED to create a timer for this user process
     *      2. We want to implement a message queue after and have it synchronize with the oss process.
     *
     */

    // we'll be using arriveTime from our PCB to determine if we should terminate.

    printf("I am child exec'd now my pid is: %ld\n", (long) getpid());
    int random;
    while(1){
        //our whole basis of the user process is to wait for the "START" from OSS
        if( receiveMsg(&message, pcb->userPID, getCMsgID(), true) == -1 ){
            //printf("./user: did not get a message\n");
        } else {

            // processes should run atleast 1 second before it terminates. I'm going to be taking the idea from Jared to
            // greenlight termination.

            if(!terminatable){
                //we'll be running constant checks to see if our process is terminatable
                if( (int)pcb->totalSysTime.sec >= 1 ){
                    //this means we've hit our 1 second threshold and we can terminate
                    terminatable = true;
                }
            }

            //we've received the message to go ahead from OSS now we'll start requesting resource or terminate.
            printf("user: totalSysTime: %ld:%ld\n", pcb->totalSysTime.sec, pcb->totalSysTime.ns);

            // this way we'll be either getting a 1, 2, or 3.
            // We'll be releasing resources if we were able to get our resources allocated.
            do{
                //we'll keep rolling until we have something that makes sense.
                random = (rand() % 3) + 1;
            } while ( ( random == 2 && !haveResources ) || ( random == 3 && !terminatable ) );
            //we'll be changing up the way we do things with requesting for resources and terminating.
            if( random == 1 ){
                // request resources
                request();
            } else if ( random == 2 ){
                // release resources
                release();
            } else if ( random == 3 ){
                // terminating process
                terminate();
            }
        }
    }
}

void userInit( int argc, char** argv ){
    initShm();
    initMsq();
    int localPID;
    srand(time(NULL) + getpid());
    printf("argv[1]: %s\n", argv[1]);
    ushm = getSharedMemory();
    localPID = atoi( argv[1] );
    pcb = getPTablePCB( localPID );
    pcb->localPID = localPID;
    pcb->userPID = getpid();
    printf("localPid: %d\n", pcb->localPID);
    int i;
    //we'll have user initialize max resources.
    for( i = 0; i < 20; i++ ){
        pcb->reqMax[i] = rand() % ( ushm->available[i] + 1 );           //this way we'll have random 0 - 10 for max resources
        pcb->allocate[i] = 0;
    }
}

void request(){
    printf("./user %d: REQUESTING...\n", pcb->localPID);
    int i;

    for( i = 0; i < 20; i++){
        //this will give us a random request of resources from 0 to max allowed (max - allocate).
        pcb->requests[i] = rand() % ( 1 + pcb->reqMax[i] - pcb->allocate[i] );
    }

    if (sendMsg(&message, "REQUEST", pcb->userPID, getPMsgID(), true) == -1) {
        printf("user: did not send a message\n");
    } else {
        //message sent!
        printf("user %ld: REQUEST msg sent\n", (long) getpid());
        //waiting to receive the go ahead on resources
        do {
            if (receiveMsg(&message, pcb->userPID, getCMsgID(), true) == -1) {
                printf("./user: did not get a message\n");
            } else {
                if (strcmp(message.msg, "GRANTED") == 0) {
                    //we've received the go ahead with our resources
                    printf("resources granted!\n");
                    haveResources = true;
                } else {
                    haveResources = false;
                }
            }
        }while( haveResources == false );
    }
}

void release(){
    //sending a message to oss asking to be unblocked
    if (sendMsg(&message, "RELEASE", pcb->userPID, getPMsgID(), true) == -1) {
        printf("user: did not send a message\n");
    } else {
        //message sent!
        printf("user %ld: RELEASE msg sent\n", (long) getpid());
    }
}

void terminate(){
    printf("./user: terminating...\n");
    int randomPercent = (rand() % 100) + 1;
    char buf[BUF_LEN];
    //sending a message to oss that user process is terminating.
    if (sendMsg(&message, "TERMINATE", pcb->userPID, getPMsgID(), true) == -1) {
        printf("user: did not send a message\n");
    } else {
        //message sent!
        printf("user %ld: TERMINATE msg sent\n", (long) getpid());
        exit((int)pcb->localPID);
    }

}