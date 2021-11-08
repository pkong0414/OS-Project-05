//sharedHandler.c

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "sharedHandler.h"

//permissions
#define PERM (IPC_CREAT | S_IRUSR | S_IWUSR)

static key_t myKey;
static int semID;
static int shmID;
static int parentmsgID;
static int childmsgID;
static key_t parentmsgKey;
static key_t childmsgKey;
static sharedMem *shmaddr = NULL;

// ******************************************** System Process functions ***********************************************

// ******************************************** Shared Memory functions ************************************************

void initShm(){
    //********************* SHARED MEMORY PORTION ************************

    if((myKey = ftok(".",1)) == (key_t)-1){
        //if we fail to get our key.
        fprintf(stderr, "Failed to derive key from filename:\n");
        exit(EXIT_FAILURE);
    }
    printf("derived key from, myKey: %d\n", myKey);

    if((shmID = shmget(myKey, sizeof(sharedMem), PERM)) == -1){
        perror("Failed to create shared memory segment\n");
        exit(EXIT_FAILURE);
    } else {
        // created shared memory segment!
        printf("created shared memory!\n");

        if((shmaddr = (sharedMem *)shmat(shmID, NULL, 0)) == (void *) -1) {
            perror("Failed to attach shared memory segment\n");
            if(shmctl(shmID, IPC_RMID, NULL) == -1) {
                perror("Failed to remove memory segment\n");
            }
            exit(EXIT_FAILURE);
        }
        // attached shared memory
        printf("attached shared memory\n");
    }
    //****************** END SHARED MEMORY PORTION ***********************
}

// ******************************************** Semaphore functions *************************************************

void initSem(){
    if((semID = semget(myKey, 1, PERM | IPC_CREAT)) == -1){
        perror("Failed to create semaphore with key\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Semaphore created with key\n");
    }
    //setsembuf(&semW, 0, -1, 0);
    //setsembuf(&semS, 0, 1, 0);
}

sharedMem *getSharedMemory(){
    return shmaddr;
}

PCB *getPTablePCB( int index ){
    return &shmaddr->pTable[index];
}

PCB *getPTablePID( pid_t pid ){
    int i;
    for( i = 0; i < 19; i++ ){
        printf("%d pid: %ld\n", i, shmaddr->pTable[i].userPID);
        if( pid == shmaddr->pTable[i].userPID )
            return &shmaddr->pTable[i];
    }

    printf("couldn't find a match\n");
}

void clearAProcessTable( int index ){
    PCB *pcb = &shmaddr->pTable[index];
    pcb->userPID = 0;
    pcb->localPID = -1;
    pcb->priority = -1;
}

int removeShm(){
    int error = 0;
    printf("Detaching shared memory...\n");

    if(shmdt(shmaddr) == -1){
        printf("Failed to detach shared memory address\n");
        error = errno;
    }
    if((shmctl(shmID, IPC_RMID, NULL) == -1) && !error){
        printf("Failed to detach shared memory id\n");
        error = errno;
    }
    if(!error){
        return 0;
    }
    errno = error;
    return -1;
}

int getFreePTableIndex(){
    int i;
    for(i = 0; i < 18; i++){
        if( shmaddr->pTable[i].userPID == 0 )
            return i;
    }
    return -1;
}

int removeSem(){
    printf("Removing semaphore...\n");
    return semctl(semID, 0, IPC_RMID);
}

void semWait(struct sembuf semW){
    if (semop(semID, &semW, 1) == -1) {
        perror("semop");
        exit(1);
    }
}

void semSignal(struct sembuf semS){
    if (semop(semID, &semS, 1) == -1) {
        perror("semop");
        exit(1);
    }
}

int r_semop(struct sembuf *sops, int nsops){
    while(semop(semID, sops, nsops) == -1){
        if(errno != EINTR){
            return -1;
        }
    }
    return 0;
}

void setsembuf(struct sembuf *s, int num, int op, int flg){
    s->sem_num = (short)num;
    s->sem_op = (short)op;
    s->sem_flg = (short)flg;
    return;
}

// ******************************************** Message Queue ********************************************************

int initMsq(){
    if((parentmsgKey = ftok(".",'p')) == (key_t)-1){
        //if we fail to get our key.
        fprintf(stderr, "Failed to derive key from filename:\n");
        exit(EXIT_FAILURE);
    }

    if((parentmsgID = msgget(parentmsgKey, PERM)) == -1){
        perror("Failed to create parent message queue\n");
        exit(EXIT_FAILURE);
    }
    printf("Message Queue for parent created\n");

    if((childmsgKey = ftok(".",'c')) == (key_t)-1){
        //if we fail to get our key.
        fprintf(stderr, "Failed to derive key from filename:\n");
        exit(EXIT_FAILURE);
    }

    if((childmsgID = msgget(childmsgKey, PERM)) == -1){
        perror("Failed to create child message queue\n");
        exit(EXIT_FAILURE);
    }
    printf("Message Queue for child created\n");
    return;
}

int receiveMsg(Message *message, pid_t pid, int msgid, bool waiting){
    if( msgrcv(msgid, message, sizeof(Message), pid, waiting ? 0: IPC_NOWAIT) > -1 ){
        //received message!
        return 0;
    } else {
        perror("receive Message.");
        return -1;
    }
}

int sendMsg(Message *message, char *msg, pid_t pid, int msgid, bool waiting){
    message->type = pid;
    strncpy(message->msg, msg, BUF_LEN);
    if( msgsnd(msgid, message, sizeof(Message), waiting ? 0: IPC_NOWAIT) > -1 ){
        //sent message!
        printf("user %ld: message sent.\n", (long)pid);
    } else {
        perror("send Message.");
        return -1;
    }
}

int removeMsq(){
    msgctl(parentmsgID, IPC_RMID, NULL);
    printf("removed parent message queue\n");

    msgctl(childmsgID, IPC_RMID, NULL);
    printf("removed child message queue\n");
    return 0;
}

int getPMsgID(){
    return parentmsgID;
}

int getCMsgID(){
    return childmsgID;
}

//time handling functions
void addTime(Time* time, long timeValue){
    time->ns += timeValue;
    while(time->ns >= SECOND) {
        time->ns -= SECOND;
        time->sec += 1;
    }
}

void clearTime( Time *time ){
    time->sec = 0;
    time->ns = 0;
}

void copyTime(Time *src, Time *dest){
    dest->sec = src->sec;
    dest->ns = src->ns;
}

int compareLeftGtrEqTime( Time time1, Time time2 ){
    //this function will two times and will tell if the left is greater than or equal to the right.
    if( time1.sec > time2.sec){
        //simplest case we have left side time seconds being greater
        return 1;
    } else if ( time2.sec > time1.sec ){
        //else if time2's second is bigger, then it is false
        return 0;
    } else if( time1.sec == time2.sec && time1.ns >= time2.ns ){
        //We now consider cases of a tie in seconds so we must compare nanoseconds
        return 1;
    } else {
        //failed all the cases so we know that time2 is simply bigger.
        return 0;
    }
}

void calcPSysTime( PCB *pcb, Time sysTime ){
    pcb->totalSysTime.ns = ( sysTime.ns - pcb->arriveTime.ns );
    pcb->totalSysTime.sec = ( sysTime.sec - pcb->arriveTime.sec );
}