#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "queue.h"
#include "sharedHandler.h"
#include "signalHandler.h"

#define IOCHANCE 50

typedef struct BITFIELD{
    unsigned int bit: 1;                                        //an array of 18 to handle local pids. Value: 1 or 0
} Bitfield;

void parsingArgs(int argc, char**argv);
void printOSSInfo();
//basic local pid functions
void initPid();
void removePidFromIndex(int index);
int getAvailpid();
bool pidFull();
void printBitfield();

//oss functions
void createChild();
void initOSS();
void initPClock(PCB *pcb);
void initQueues();
void exitOSS();
void ossSimulation();
void assignPid2PTable(int index, pid_t pid);
void handleRunningProcess();
void handleRequest( PCB* pcb );
void handleTerminate( PCB* pcb );
void handleRelease( PCB* pcb );
void bankersAlgo();
void printSystemResources();
void printAllocatedResources();
void passTime();
void writeLog( const char* logmsg );

//Queues
Queue *safeQueue;
Queue *blockedQueue;
Queue *messageQueue;

// GLOBALS
enum state{idle, want_in, in_cs};
int opt, timer, nValue, tValue;                     //This is for managing our getopts
int currentConcurrentProcesses = 1;                 //Initialized as 1 since the main program is also a process.
int totalProcessesCreated = 0;                      //number of created process
int totalExitedProcess = 0;
int totalBlockedProcess = 0;
int totalHighPriorityProc = 0;
int totalLowPriorityProc = 0;
int waitStatus;                                     //This is for managing our processes
sharedMem *shm;                                     //shared memory object
int semID;                                          //SEMAPHORE ID
struct sembuf semW;
struct sembuf semS;
Message message;
PCB *activePCB = NULL;
Bitfield availPids[18];
int allowedProcesses = 50;                           //Number we will use to define total allowed processes [default: 50]
long maxTimeBetweenNewProcsNS = 100;                //we'll use this nanosecs to determine when oss creates a new process
long maxTimeBetweenNewProcsSecs = 1;                //we'll use this seconds determine when oss creates a new process

//log globals
int grantedCount = 0;
int totalLines = 0;                                 //this will keep track of lines written. Limit is 10000
char *logName = "log.log";
bool verbose = false;                               //if this is turned on, our log will report everything
                                                    //else we'll only report certain operations.

//OSS timers
Time spawnTime = {.sec = 0,.ns = 0};
Time idleTime = {.sec = 0, .ns = 0};
Time totalWaitTime = {.sec = 0, .ns = 0};


int main(int argc, char**argv) {
    //we'll be parsing our arguments here
    parsingArgs( argc, argv);

    //initializing shared memory and semaphore
    initOSS();

    if( setupUserInterrupt() == -1 ){
        perror( "failed to set up a user kill signal.\n");
        return 1;
    }

    //setting up interrupts after parsing arguments
    if (setupinterrupt() == -1) {
        perror("Failed to set up handler for SIGALRM");
        return 1;
    }

    if (setupitimer(MAX_SECONDS) == -1) {
        perror("Failed to set up the ITIMER_PROF interval timer");
        return 1;
    }

    ossSimulation();

    exitOSS();
}

void parsingArgs(int argc, char** argv){
    if( argc < 1 ){
        printf("Usage: %s [-h] [-t <seconds. DEFAULT:3 MAX:100>] [-l <logName>.log ]\n", argv[0]);
        printf("This program is a license manager, part 2. We'll be doing using this to learn about semaphores\n");
        exit(EXIT_FAILURE);
    }

//    while((opt = getopt(argc, argv, "")) != -1) {
//        switch (opt) {
//            default: /* '?' */
//                printf("%s: ERROR: parameter not recognized.\n", argv[0]);
//                fprintf(stderr, "Usage: %s [-h] [-t <seconds. DEFAULT:100>] [-l <log name>]\n", argv[0]);
//                exit(EXIT_FAILURE);
//        }
//    }


    /* END OF GETOPT */
}

void printOSSInfo(){
    printBitfield();
    printf("printing safeQueue:\n");
    printQueue( safeQueue );
    printf("printing blockedQueue:\n");
    printQueue( blockedQueue );

    exitOSS();
}

void initPid() {
    int i;
    for(i = 0; i < 18; i++) {
        availPids[i].bit = 0;
    }
}

void removePidFromIndex( int index ){
    availPids[index].bit = 0;
}

int getAvailPid() {
    int i;
    for(i = 0; i < 18; i++) {
        if( availPids[i].bit == 0 ) {
            availPids[i].bit = 1;
            return i;
        }
    }
    return -1;
}

bool pidFull(){
    int i;
    for(i = 0; i < 18; i++){
        if( availPids[i].bit == 0 ){
            return false;
        }
    }
    return true;
}

void printBitfield(){
    int i;
    for(i = 0; i < 18; i++){
        printf("bit[%d]: %d\n", i, availPids[i]);
    }

}

void createChild(){
    char logmsg[BUF_LEN];
    pid_t pid;
    pid_t childPid;
    char buffer[20];
    int pTableID;
    int priorityRandom;
    if( !pidFull() ) {
        if (currentConcurrentProcesses <= MAX_PROC && (totalProcessesCreated < allowedProcesses)) {
            pTableID = getAvailPid();
            if ((pid = fork()) == -1) {
                perror("Failed to create grandchild process\n");
                if (removeShm() == -1) {
                    perror("Failed to destroy shared memory segment");
                }
                exit(EXIT_FAILURE);
            }
            currentConcurrentProcesses++;
            totalProcessesCreated++;

            // made a child process!
            if (pid == 0) {
                /* the child process */
                //debugging output
                childPid = getpid();                                        //getting child pid
                char buf[BUF_LEN];
                sprintf(buf, "%d\n", pTableID);
                printf("current concurrent process %d: myPID: %ld localPID: %s\n", currentConcurrentProcesses, (long) childPid, buf);
                execl("./user_proc", "user_proc", buf, NULL);
            }

            /* parent process */

            //we are initializing the PCB inside the PTable while we have a chance.
            printf("parent process. totalProcessesCreated: %d\n", totalProcessesCreated);
            printf("parent process. Child's localPID: %d\n", pTableID);
            PCB *newPCB;
            printf("pTableID: %d\n", pTableID);
            assignPid2PTable(pTableID, pid);
            sprintf(buffer, "%d\n", pTableID);
            printf("buffer: %s\n", buffer);
            sprintf(buffer, "%d\n", pTableID);
            printf("buffer: %s\n", buffer);
            printf("child's pid: %ld\n", (long) pid);

            newPCB = getPTablePCB(pTableID);
            printf("oss: newPCB's pid: %ld\n", (long) newPCB->userPID);
            initPClock(newPCB);
            push( messageQueue, pTableID);                  //pushing a new process to message queue.
            // we don't need to get process priority, for this one. So let's just send a message to tell them to run.

        }
        return;
    }
}

void initOSS(){
    //initializing shared memory and message queues
    initShm();
    initMsq();
    //initializing bit field
    initPid();
    //initializing Queues
    //Queues
    safeQueue = initQueue();
    blockedQueue = initQueue();
    messageQueue = initQueue();
    srand(time(NULL));

    shm = getSharedMemory();                    //getting shared memory

    //giving us available resources.
    int i;
    for( i = 0; i < REC_MAX; i++ ){
        shm->available[i] = (rand() % 10 ) + 1;                 //this will give us avail resources 1 - 10 inclusive.
    }
    //randomly deciding which resource will be shared resource
    //We assume shared resources is anything from 20 +- 5(this will be decided random)
    //calculating sharedCount
    int sharedCount = (rand() % (25 - (25 - 15 )) + 15);
    int resource;
    for( i = 0; i < sharedCount; i++){
        while(true){
            resource = rand() % REC_MAX;                        //getting the random resource we'll be marking shared.
            if(!shm->shared[resource]){
                shm->shared[resource] = true;
                break;
            }
        }
    }

    for( i = 0; i < MAX_PROC; i++){
        shm->pTable[i].localPID = -1;
        shm->pTable[i].userPID = -1;
    }
}

void initPClock( PCB *pcb ){
    clearTime( &pcb->totalSysTime);
    clearTime( &pcb->totalCpuTime);
//    clearTime( &pcb->burstTime);
//    clearTime( &pcb->waitTime);
//    clearTime( &pcb->totalWait);
//    clearTime( &pcb->timeLimit);
//    copyTime( &shm->sysTime, &pcb->arriveTime );
}

void exitOSS(){
    //removing shared memory and message queues
    removeShm();
    removeMsq();
    removeQueue( safeQueue );
    removeQueue( blockedQueue );
    exit(EXIT_SUCCESS);
}

void ossSimulation(){
    //testing out bitfields and queues
//    printBitfield();
//    printQueue( highQueue );
//    printQueue( lowQueue );
//    exitOSS();

    while(1) {
        passTime();
        while ((currentConcurrentProcesses < MAX_PROC) || (totalProcessesCreated < allowedProcesses)) {
            if( pidFull() ) {
                break;
            }                          //we don't have any available pid slots
            //we want to increment the time until it is actually time to create processes
            //we are following the format of 1.xx,
            // where 1 second is guaranteed
            // xx depends on nanoseconds rolled from 1-1000

            printf( "oss: system Time: %ld:%ld\n", shm->sysTime.sec, shm->sysTime.ns );
            if( (spawnTime.sec <= shm->sysTime.sec ) &&
                (spawnTime.ns <= shm->sysTime.ns ) ){
                //resetting the spawnTime now.
                copyTime( &shm->sysTime, &spawnTime );
                addTime( &spawnTime, (long)( ((rand() % maxTimeBetweenNewProcsSecs) + 1) * SECOND) );
                addTime( &spawnTime, (long)( ((rand() % maxTimeBetweenNewProcsNS) + 1)) );
                createChild();
                break;
//                printOSSInfo();
            } else {
                passTime();
            }

        }
        int status;
        handleRunningProcess();

        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0){
            //we are waiting for our processes to end

            int localpid = WEXITSTATUS(status);
            currentConcurrentProcesses--;
            totalExitedProcess++;
            printf("current concurrent process %d\n", currentConcurrentProcesses);
            printf("Child process successfully exited with status: %d\n", localpid);
            printf("total processes created: %d\n", totalProcessesCreated);
            //*********************************** EXIT SECTION **********************************************

            //we want to clear process tables and perform calculations to make the report


            //******************************** REMAINDER SECTION ********************************************
        }
        if(totalExitedProcess == allowedProcesses){
            //we'll break out of our loop when we are at created process limit
            break;
        }
    }

    //******************************************* REPORT SECTION ***********************************************


    return;
}

void assignPid2PTable( int index, pid_t pid){
        shm->pTable[index].userPID = pid;
}

void handleRunningProcess(){
    // receiving a message from processes. This will be unlike project 4 where we have the benefit of one process
    // Since we'll be receiving request from multiple processes.
    // We also do not have any single active process, so we must look for the process itself.
    // By having a messageQueue, we'll be telling processes to move along.

    //we'll be storing localpids inside of messageQueue, we'll use that to grab the PCB in our PTable.
    int currentLocalPid;
    int i;

    printQueue(messageQueue);

    for(i = 0; i < messageQueue->size; i++) {
        // we only got 18 processes, and we'll rotate through the queue receive messages
        // when we hit the end of the queue we'll be doing other things.
        if( messageQueue->arr[i] == -1 )
            continue;                                       //skipping if we find the queue slot empty.

        currentLocalPid = messageQueue->arr[i];
        PCB *pcb = getPTablePCB( currentLocalPid );
        printf("inside the resource manager, looking for... %ld\n", pcb->userPID);
        //we'll be telling the processes to go first before we process their stuff.
        sendMsg( &message, "START", pcb->userPID, getCMsgID(), true );

        if (receiveMsg(&message, pcb->userPID, getPMsgID(), true) == -1) {
            printf("oss: did not receive a message\n");
        } else {
            //message received!
            printf("message received!\n");
            if (strcmp(message.msg, "TERMINATE") == 0) {
                printf("oss: TERMINATE msg received from %ld\n", message.type);
                handleTerminate(pcb);
            } else if (strcmp(message.msg, "REQUEST") == 0) {
                printf("oss: REQUEST msg received from %ld\n", message.type);
                handleRequest(pcb);
            } else if (strcmp(message.msg, "RELEASE") == 0) {
                printf("oss: RELEASE msg received from %ld\n", message.type);
                handleRelease(pcb);
            }
            bankersAlgo();
        }
    }
    passTime();
}

void handleRequest( PCB* pcb ){
    //we know how much is being requested by the specific pcb, so we'll use that to make the according allocation.
    int i;
    int tempList[REC_MAX];
    char msg[BUF_LEN];
    bool resourceAvail = true;
    printSystemResources();
    for( i = 0; i < REC_MAX; i++ ){
        if( shm->shared[i] ){
            //since we know this is a shared resources, we don't have to worry about availability.
            continue;
        }
        if( pcb->requests[i] <= shm->available[i] ){
            //this means we have enough available to handle this
            tempList[i] = pcb->requests[i];
        } else {
            resourceAvail = false;
        }
    }

    if( resourceAvail ){
        // if memory is available!
        int j;
        for( j = 0; j < REC_MAX; j++){
            pcb->allocate[j] += tempList[j];
            shm->available[j] -= tempList[j];
            pcb->requests[j] = 0;
            if(tempList[j] == 0)
                continue;                               //we are going to skip this if there isn't any requests made.

            if(!verbose){
                // with verbose off we only indicate what resources are requested granted and avail resources
                sprintf( msg, "Process:%d Granted R%d:%d\n", pcb->localPID, j, tempList[j]);
                writeLog( msg );
            } else {
                // with verbose on we'll log what resources are released
                sprintf( msg, "Process:%d Granted R%d:%d\n", pcb->localPID, j, tempList[j]);
                writeLog( msg );
            }
        }
        //resource granted.
        printf("./oss: Resource Granted. Sending message to process: %ld\n", pcb->userPID);
        grantedCount++;
        sendMsg( &message, "GRANTED", pcb->userPID, getCMsgID(), true );
    } else {
        // if memory isn't immediately available.
        printf("./oss: we don't have enough resources. User:%ld is blocked for now.\n", pcb->userPID);
        sprintf( msg, "Process:%d Denied resources and blocked for now\n", pcb->localPID);
        writeLog( msg );
        push(blockedQueue, pcb->localPID);
    }
    if(grantedCount == 20){
        printAllocatedResources();
        grantedCount = 0;                               //we are resetting the granted count.
    }

}

void handleTerminate( PCB* pcb ){
    int i;
    char msg[BUF_LEN];
    int localPid = pcb->localPID;
    //we have a terminated process. This pcb will release its memory before printing out its stats.
    handleRelease( pcb );
    sprintf( msg, "Process:%d terminated at %ld:%ld\n", pcb->localPID, shm->sysTime.sec, shm->sysTime.ns);
    writeLog( msg );
    clearTime( &pcb->totalCpuTime);

    for(i = 0; i < REC_MAX; i++){
        pcb->requests[i] = 0;
        pcb->reqMax[i] = 0;
    }
    //we'll need to pop this process from the messageQueue tables.
    for(i = 0; i < MAX_PROC; i++){
        if( messageQueue->arr[i] == localPid ){
            //we've found our localPid in this messageQueue
            messageQueue->arr[i] = -1;
        }
    }

    removePidFromIndex( localPid );
    clearAProcessTable( localPid );


}

void handleRelease( PCB* pcb ){
    //A Process is releasing its resources
    int i;
    int tempVal;
    char msg[BUF_LEN];
    for( i = 0; i < REC_MAX; i++ ){
        if(shm->shared[i] == true){
            pcb->allocate[i] = 0;
            continue;
        } else {
            shm->available[i] += pcb->allocate[i];
            tempVal = pcb->allocate[i];
            pcb->allocate[i] = 0;
        }
        if(!verbose){
            // with verbose off we only indicate what resources are requested granted and avail resources
            printf("Process:%d Released R%d:%d\n", pcb->localPID, i, tempVal);
            sprintf( msg, "Process:%d Released R%d:%d\n", pcb->localPID, i, tempVal);
            writeLog( msg );
        } else {
            // with verbose on we'll log what resources are released
            printf("Process:%d Released R%d:%d\n", pcb->localPID, i, tempVal);
            sprintf( msg, "Process:%d Released R%d:%d\n", pcb->localPID, i, tempVal);
            writeLog( msg );
        }
    }
}

void bankersAlgo(){
    // We'll be performing a deadlock check using banker's algo now.

    // When verbose is off, it should only log when a deadlock is detected, and how it was resolved.
    // This means it is the only function not affected by verbose
    int i,j,k;
    int p = 0;

    bool safe;
    bool found;
    char msg[BUF_LEN];
    int safeStates[blockedQueue->currentCapacity];
    int localPid[blockedQueue->currentCapacity];
    // this is what we'll use to calculate our safety.
    int potential[blockedQueue->currentCapacity][REC_MAX];                          //potential requests
    int copyAlloc[blockedQueue->currentCapacity][REC_MAX];
    int copyReq[blockedQueue->currentCapacity][REC_MAX];
    int copyAvail[REC_MAX];
    int totalPotentialReq;

    if( blockedQueue->currentCapacity == 0 ){
        return;
    }

    for( i = 0; i < REC_MAX; i++ ) {
        copyAvail[i] = shm->available[i];
    }

    for( i = 0; i < blockedQueue->currentCapacity; i++ ){
        // we'll be looking at our blockedQueue to handle deadlock

        // idea is if the potential requests of all the processes exceed the available resources
        // available, we'll be deadlocked!

        //we'll grab the first pid.
        localPid[p] = findFrontPID( blockedQueue );

        for( j = 0; j < REC_MAX; j++ ) {
            copyAlloc[i][j] = shm->pTable[localPid[p]].allocate[j];
            copyReq[i][j] = shm->pTable[localPid[p]].requests[j];

            //we'll calculate our potential requests by taking max - allocate. This will be for all resources
            //for all processes.
            potential[i][j] = shm->pTable[localPid[p]].reqMax[j] - shm->pTable[localPid[p]].allocate[j];
        }
        //incrementing process index
        p++;
        //this will rotate the queue all around.
        rotateQueue( blockedQueue );
    }

    // we've completed our copy; so now we'll find our safety sequence. Since we are handling processes by fulfilling
    // requests and immediately taking the resources from the processes finished, we'll be able to further allocate to
    // others.

    //resetting process count to 0
    p = 0;
    for( i = 0; i < blockedQueue->currentCapacity; i++ ) {
        safe = false;
        found = false;
        if( !found ){
            for( j = 0; j < REC_MAX; j++){
                if( potential[i][j] > copyAvail[j] ) {
                    sprintf( msg, "Deadlock Detected at %d:%d\n", shm->sysTime.sec, shm->sysTime.ns);
                    writeLog( msg );
                    break;
                    // if any potential requests of all processes exceed the available units and thus all processes are
                    // deadlocked. This means we can skip this one and look for another thing.
                }
            }

            if( j == REC_MAX ){
                // this means we've made it to the end of the checklist on avail units vs potential request, thus safe.
                for( k = 0; k < REC_MAX; k++ ){
                    copyAvail[k] += copyAlloc[i][k];
                }

                //pushing to the safe Queue
                push( safeQueue, localPid[i] );
                while( localPid[i] != blockedQueue->arr[blockedQueue->front])
                    rotateQueue(blockedQueue);
                pop(blockedQueue);
                found = true;
                safe = true;
            }
        }
    }

    if( safe ) {
        sprintf(msg, "Safe sequence found!\n");
        writeLog(msg);

        int currentLPid;
        PCB *pcb;
        for (i = 0; i < safeQueue->currentCapacity; i++) {
            // we have a safe sequence, so we'll be popping in the order it was meant. The last sequence will also release.
            currentLPid = pop(safeQueue);
            pcb = getPTablePCB(currentLPid);
            sprintf(msg, "Granting Process:%d after banker's algo\n", pcb->userPID);
            writeLog(msg);
            sendMsg(&message, "GRANTED", pcb->userPID, getCMsgID(), true);
        }
        removeQueue(safeQueue);
        return;
    }

    if( !safe ){
        //we'll remove the safeQueue since we know we haven't found anything.
        removeQueue(safeQueue);
        // we know there isn't a safe way to handle this.
        return;
    }
}

void printSystemResources(){
    int i, j;
    for( i = 0; i < REC_MAX; i++ ){
        //printing out the top row of resources
        printf(" R%*d", 1, i);
    }
    printf("\n");
    for( j = 0; j < REC_MAX; j++){
        //printing the resources now
        if( j < 10) {
            printf("%*d", 3, shm->available[j]);
        } else {
            printf(" %*d", 3, shm->available[j]);
        }
    }
    printf( "\n");
}

void printAllocatedResources(){
    int i, j;
    char msg[BUF_LEN];

    for( i = 0; i < REC_MAX; i++ ){
        //printing out the top row of resources
        printf("     R%d", i);
    }
    printf("\n");
//    sprintf( msg, "\n");
//    writeLog( msg );
    for( i = 0; i < MAX_PROC; i++) {
        //starting the printing of processes
        printf("P%*d", 4, i );
//        sprintf( msg, "P%*d", 4, i );
//        writeLog( msg );
        for( j = 0; j < REC_MAX; j++){
            //printing the resources now
            if( j < 10 ) {
                printf(msg, "%*d", 3, shm->pTable[i].allocate[j]);
//                sprintf(msg, "%*d", 3, shm->pTable[i].allocate[j]);
//                writeLog(msg);
            } else {
                printf(" %*d", 3, shm->pTable[i].allocate[j]);
//                sprintf(msg, " %*d", 3, shm->pTable[i].allocate[j]);
//                writeLog(msg);
            }
        }
        printf("\n");
//        sprintf( msg, "\n");
//        writeLog(msg);
    }
}

void passTime(){
    long timePassed;
    int i;
    PCB *pcb;
    timePassed = ( SECOND + ((rand() % 1000 ) + 1) );
    addTime( &shm->sysTime, timePassed );
    for(i = 0; i < messageQueue->currentCapacity; i++ ){
        if(messageQueue->arr[i] == -1){
            continue;
        }
        pcb = getPTablePCB( messageQueue->arr[i] );
        addTime( &pcb->totalSysTime, timePassed );
    }
}

void writeLog(const char* logmsg) {
    /* logmsg in the format:
    *   Time PID Iteration# of NumberOfIterations
    */
    FILE *saveFile = fopen(logName, "a+");


    //The writeLog will write the logmsg to the .log file
    if(saveFile == NULL){
        perror("log.c: ERROR");
        //return -1 if unsuccessful
        return;
    }
    else {
        if (totalLines < 10000) {
            //Now writing to file
            fprintf(saveFile, "%s", logmsg);
            //closing file
            fclose(saveFile);
            return;
        } else {
            printf("./oss: ERROR: OVER 10000 lines. Message not saved.\n");
            fclose(saveFile);
            return;
        }
    }
}