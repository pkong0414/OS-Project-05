//user.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sharedHandler.h"
#include "signalHandler.h"

#define TERMCHANCE 80
#define BLOCKCHANCEP1 50                                //I/O bound processes are more likely to get interrupted
#define BLOCKCHANCEP2 80                                //CPU bound processes are unlikely to get interrupted

sharedMem *ushm;
PCB *pcb;
Message message;

void userInit( int argc, char** argv );
void processScheduler();
void block();
void terminate();
void expire();

int main(int argc, char ** argv){
    if( setupUserInterrupt() == -1 ){
        perror( "failed to set up a user kill signal.\n");
        return 1;
    }

    userInit( argc, argv );
    int blockChance = ( pcb->priority == 1 ? BLOCKCHANCEP1: BLOCKCHANCEP2);

    /*  TO DO:
     *
     *      1. We NEED to create a timer for this user process
     *      2. We want to implement a message queue after and have it syncronize with the oss process.
     *
     *      3. We will implement the random number generator to simulate a user process.
     *
     */

    printf("I am child exec'd now my pid is: %ld\n", (long) getpid());
    int random;
    while(1){
        if( receiveMsg(&message, pcb->userPID, getCMsgID(), true) == -1 ){
            printf("./user: did not get a message\n");
        } else {
            printf("user: totalCpuTime: %ld:%ld\n", pcb->totalCpuTime.sec, pcb->totalCpuTime.ns);
            random = (rand() % 100) + 1;
            if (random > blockChance) {
                //we block
                block();
            } else if (random > TERMCHANCE) {
                //we terminate
                terminate();
            } else {
                //we expire
                expire();
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
}

void block(){
    printf("./user: blocking...\n");
    //we are using r.s to determine how long to wait
    int r = (rand() % 5) + 1;                               //gives us a random value of 1-5
    int s = (rand() % 1000) + 1;                            //gives us a random value of 1-1000
    //format is r:s,         where r is seconds [1-5] and s is nanoseconds [1-1000].
    Time rs;
    char timeUsed[BUF_LEN];
    //random will be the percent of the time quantum we've used up till block.
    int random = (rand() % 100) + 1;                        //this gives us a random percentile for quantum used.
    //we are generating a timestamp of when we should be unblocked.
    copyTime(&ushm->sysTime, &rs);
    rs.sec += r;
    rs.ns += s;
    //sending a message from blocked processes
    if (sendMsg(&message, "BLOCK", pcb->userPID, getPMsgID(), false) == -1) {
        printf("user: did not send a message\n");
    } else {
        //message sent!
        printf("user %ld: BLOCK msg sent\n", (long) getpid());
    }

    sprintf(timeUsed, "%d", random);
    //we need to send a report to the OSS about how much time we've used.
    if (sendMsg(&message, timeUsed, pcb->userPID, getPMsgID(), true) == -1) {
        printf("user: did not send a message\n");
    } else {
        //message sent!
        printf("user %ld: %s msg sent\n", (long)getpid(),timeUsed);
    }

    //we've randomized rs and also sent block message. We need to wait until sysTime is equal or greater than rs
    while ( (ushm->sysTime.sec <= rs.sec) && (ushm->sysTime.ns <= rs.ns) );

    //sending a message to oss asking to be unblocked
    if (sendMsg(&message, "UNBLOCK", pcb->userPID, getPMsgID(), false) == -1) {
        printf("user: did not send a message\n");
    } else {
        //message sent!
        printf("user %ld: UNBLOCK msg sent\n", (long) getpid());
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
    }

    sprintf(buf, "%d", randomPercent);
    //we need to send a message about the percent of our quantum used.
    if (sendMsg(&message, buf, pcb->userPID, getPMsgID(), true) == -1) {
        printf("user: did not send a message\n");
    } else {
        //message sent!
        printf("user %ld: TERMINATE msg sent\n", (long) getpid());
    }
    exit(pcb->localPID);
}

void expire(){
    printf("./user: expiring...\n");
    //sending a message to oss that user used up its time slice
    if (sendMsg(&message, "EXPIRED", pcb->userPID, getPMsgID(), true) == -1) {
        printf("user: did not send a message\n");
    } else {
        //message sent!
        printf("user %ld: EXPIRED msg sent\n", (long) getpid());
    }
}