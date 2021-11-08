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
void handleBlockedProcess();
void handleTerminateProcess();
void handleExpiredProcess();
void handleUnblock();
void queueProcess();
void scheduleProcess();
void passTime();
void makeActiveProcess( PCB *pcb);
void writeLog( const char* logmsg );

//Queues
Queue *highQueue;
Queue *lowQueue;
Queue *blockedQueue;

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
int allowedProcesses = MAX_TOTAL_PROC;              //Number we will use to define total allowed processes [default: 50]
long maxTimeBetweenNewProcsNS = 100;                //we'll use this nanosecs to determine when oss creates a new process
long maxTimeBetweenNewProcsSecs = 1;                //we'll use this seconds determine when oss creates a new process

//log globals
int totalLines = 0;                                 //this will keep track of lines written. Limit is 10000
FILE *saveFile;
char *logName;


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

    shm = getSharedMemory();                    //getting shared memory

    ossSimulation();

    exitOSS();
}

void parsingArgs(int argc, char** argv){
    if( argc < 2 ){
        printf("Usage: %s [-h] [-t <seconds. DEFAULT:3 MAX:100>] [-l <logName>.log ]\n", argv[0]);
        printf("This program is a license manager, part 2. We'll be doing using this to learn about semaphores\n");
        exit(EXIT_FAILURE);
    }

    while((opt = getopt(argc, argv, "ht:l")) != -1) {
        switch (opt) {
            case 'h':
                //This is the help parameter. We'll be printing out what this program does and will end the program.
                //If this is entered along with others, we'll ignore the rest of the other parameters to print help
                //and end the program accordingly.
                printf("Usage: %s [-h] [-t <seconds. DEFAULT:3 MAX:100>] [-l <logName>.log ]\n", argv[0]);
                printf("This program is a license manager, part 2. We'll be doing using this to learn about semaphores\n");
                exit(EXIT_SUCCESS);
            case 't':
                if (!isdigit(argv[2][0])) {
                    //This case the user uses -t parameter but entered a string instead of an int.
                    printf("value entered: %s\n", argv[2]);
                    printf("%s: ERROR: -t <number of seconds before timing out>\n", argv[0]);
                    exit(EXIT_FAILURE);
                } else {
                    // -t gives us the number of seconds to our timer.
                    tValue = atoi(optarg);
                    // we will check to make sure nValue is 1 to 20.
                    if (tValue < 1) {
                        printf("%s: processes cannot be less than 1. Resetting to default value: 3\n", argv[0]);
                        tValue = MAX_SECONDS;
                    } else if (tValue > 100) {
                        printf("%s: max time allowed is 100. Setting tValue to 100.\n", argv[0]);
                    }
                    printf("tValue: %d\n", tValue);
                    break;
                }
            case 'l':
                printf("%s\n", argv[4] );
                if (!isalpha(argv[4][0])) {
                    //this case the user uses -l parameter but entered a non alphabet.
                    logName = "logfile.log";
                    printf("invalid input. Changing logfile to: %s\n", logName);
                    break;
                } else {
                    logName = argv[4];
                    sprintf(logName, "%s.log", logName);
                }
                printf("logName: %s\n", logName);
                break;
            default: /* '?' */
                printf("%s: ERROR: parameter not recognized.\n", argv[0]);
                fprintf(stderr, "Usage: %s [-h] [-t <seconds. DEFAULT:100>] [-l <log name>]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    } /* END OF GETOPT */
}

void printOSSInfo(){
    printBitfield();
    printf("printing highQueue:\n");
    printQueue( highQueue );
    printf("printing lowQueue:\n");
    printQueue( lowQueue );
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
        pTableID = getAvailPid();
        if (currentConcurrentProcesses <= MAX_PROC) {
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
                execl("./user", "user", buf, NULL);
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

            if((priorityRandom = (rand() % 100) + 1) > IOCHANCE ) {
                //this process is IO bound
                newPCB->priority = 0;
                totalHighPriorityProc++;
                push( highQueue, pTableID );
                sprintf(logmsg, "oss: new I/O process created at: %ld:%ld\n", shm->sysTime.sec, shm->sysTime.ns);
                //writeLog( logmsg );
                return;
            } else {
                //this process is CPU bound
                newPCB->priority = 1;
                totalLowPriorityProc++;
                push( lowQueue, pTableID );
                sprintf(logmsg, "oss: new CPU process created at: %ld:%ld\n", shm->sysTime.sec, shm->sysTime.ns);
                //writeLog( logmsg );
                return;
            }
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
    highQueue = initQueue();
    lowQueue = initQueue();
    blockedQueue = initQueue();
    srand(time(NULL));
    saveFile = fopen(logName, "w+");

    if(saveFile == NULL){
        perror("oss: ERROR");
        //return -1 if unsuccessful
        return;
    }
}

void initPClock( PCB *pcb ){
    clearTime( &pcb->burstTime);
    clearTime( &pcb->totalCpuTime);
    clearTime( &pcb->waitTime);
    clearTime( &pcb->totalWait);
    clearTime( &pcb->totalSysTime);
    clearTime( &pcb->timeLimit);
    copyTime( &shm->sysTime, &pcb->arriveTime );
}

void exitOSS(){
    //removing shared memory and message queues
    removeShm();
    removeMsq();
    removeQueue( highQueue );
    removeQueue( lowQueue );
    removeQueue( blockedQueue );
    if (fclose(saveFile) == -1) {
        perror("oss: ERROR");

        //exiting since file is unable to be closed
        exit(EXIT_FAILURE);
    } else {
        printf("file closed\n");
    }
    exit(EXIT_SUCCESS);
}

void ossSimulation(){
    //testing out bitfields and queues
//    printBitfield();
//    printQueue( highQueue );
//    printQueue( lowQueue );
//    printQueue( blockedQueue );
//    printQueue( expiredQueue );
//    exitOSS();

    while(1) {
        if(!activePCB){
            passTime();
        }
        while (currentConcurrentProcesses < 19 && !(totalProcessesCreated >= allowedProcesses)) {
            //we are adding time in our while loop.
            if( pidFull() )
                break;                          //we don't have any available pid slots
            //we want to increment the time until it is actually time to create processes
            //we are following the format of 1.xx,
            // where 1 second is guaranteed
            // xx depends on nanoseconds rolled from 1-1000
            if(!activePCB){
                passTime();
            }

            printf( "oss: system Time: %ld:%ld\n", shm->sysTime.sec, shm->sysTime.ns );
            if( (spawnTime.sec <= shm->sysTime.sec ) &&
                (spawnTime.ns <= shm->sysTime.ns ) ){
                //resetting the spawnTime now.
                copyTime( &shm->sysTime, &spawnTime );
                addTime( &spawnTime, (long)( ((rand() % maxTimeBetweenNewProcsSecs) + 1) * SECOND) );
                addTime( &spawnTime, (long)( ((rand() % maxTimeBetweenNewProcsNS) + 1)) );
                createChild();
//                printOSSInfo();
            }
        }

        handleRunningProcess();
        handleUnblock();
        scheduleProcess();

        pid_t pid = waitpid(-1, &waitStatus, WNOHANG);
        if (pid > 0){
            //we are waiting for our processes to end
            if (WIFEXITED(waitStatus)) {
                currentConcurrentProcesses--;

                printf("current concurrent process %d\n", currentConcurrentProcesses);
                printf("Child process successfully exited with status: %d\n", waitStatus);
                printf("total processes created: %d\n", totalProcessesCreated);
                //*********************************** EXIT SECTION **********************************************

                //we want to clear process tables and perform calculations to make the report
                removePidFromIndex( waitStatus );
                clearAProcessTable( waitStatus );



                //******************************** REMAINDER SECTION ********************************************

                //we finished the report and now we want to look for a new process to be active, else we'll need to make
                //a new one
            }
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
    if( shm->pTable[index].userPID == 0 )
        shm->pTable[index].userPID = pid;
}

void handleRunningProcess(){
    //receiving a message from blocked processes
    if( activePCB ) {
        PCB *pcb = activePCB;
        printf("inside the scheduler, looking for... %ld\n", (long) pcb->userPID);
        if (receiveMsg(&message, (long) pcb->userPID, getPMsgID(), true) == -1) {
            printf("oss: did not receive a message\n");
        } else {
            //message sent!
            printf("message received!\n");
            if (strcmp(message.msg, "BLOCK") == 0) {
                printf("oss: BLOCK msg received from %ld\n", message.type);
                handleBlockedProcess( pcb );
            }
            else if (strcmp(message.msg, "TERMINATE") == 0) {
                printf("oss: TERMINATE msg received from %ld\n", message.type);
                handleTerminateProcess( pcb );
            }
            else if (strcmp(message.msg, "EXPIRED") == 0) {
                printf("oss: EXPIRED msg received from %ld\n", message.type);
                handleExpiredProcess( pcb );
            }
        }
    }
}

void handleBlockedProcess( PCB* pcb ){
    char logmsg[BUF_LEN];
    int quantumPercent;
    long quantumUsed;
    Time *usedTime;
    //we'll be looking to see how much time has the user process used
    if (receiveMsg(&message, (long)pcb->userPID, getPMsgID(), true) == -1) {
        printf("oss: did not receive a message\n");
    } else {
        totalBlockedProcess++;
        quantumPercent = atoi(message.msg);
        printf("%d\n", quantumPercent);

        //we are going to modify this
        quantumUsed = (pcb->timeLimit.ns * (quantumPercent/100));
        addTime( &shm->sysTime, quantumUsed );
        addTime( &pcb->totalCpuTime, quantumUsed );
        addTime( &usedTime, quantumUsed );
        addTime( &pcb->totalSysTime, quantumUsed);
        copyTime( &usedTime, &pcb->burstTime );
        //we do not want to queue it into a normal queue, so we'll just push it directly to a blocked queue.
        sprintf(logmsg, "oss: process %ld blocked at: %ld:%ld\n", pcb->userPID, shm->sysTime.sec, shm->sysTime.ns);
        writeLog( logmsg );
        push( blockedQueue, pcb->localPID);
    }
    activePCB = NULL;
    if( !activePCB ){
        printf("active Process is blocked\n");
    }
}

void handleTerminateProcess( PCB* pcb ){
    //we'll be looking to see how many percent of the time quantum user process used before termination
    char logmsg[BUF_LEN];
    int quantumPercent;
    long quantumUsed;
    if (receiveMsg(&message, (long) pcb->userPID, getPMsgID(), true) == -1) {
        printf("oss: did not receive a message\n");
    } else {
        //we got our quantum used percent
        totalExitedProcess++;
        printf("oss: received termination from process: %ld\n", (long)pcb->userPID);
        quantumPercent = atoi(message.msg);
        printf("%d\n",quantumPercent);

        quantumUsed = ( pcb->timeLimit.ns ) * (quantumPercent/100);
        //advancing our system time with the quantumUsed
        addTime( &shm->sysTime, quantumUsed );
        //adding the quantumUsed to totalCpuTime
        addTime( &pcb->totalCpuTime, quantumUsed);
        //now we apply the finishing touches to the process since it is exiting
        //we want to note the time of exit on the user process.
        copyTime( &shm->sysTime, &pcb->exitTime );
        removePidFromIndex(pcb->localPID);
        sprintf(logmsg, "oss: process %ld exited at: %ld:%ld\n", pcb->userPID, shm->sysTime.sec, shm->sysTime.ns);
        writeLog( logmsg );
    }
    activePCB = NULL;
}

void handleExpiredProcess( PCB* pcb ){
    //While we may not necessarily need details about quantum used we still need to apply it to the system clock!
    char logmsg[BUF_LEN];
    long quantumUsed = pcb->timeLimit.ns;                        //grabbing the full timeslice.

    //adding quantum used into oss's system time
    addTime(&shm->sysTime, quantumUsed);
    //adding quantum used to user process's totalCpuTime and totalSysTime.
    addTime(&pcb->totalCpuTime, quantumUsed);

    //timestamp from current system time copied to waitTime
    copyTime( &shm->sysTime, &pcb->queuedTime);                   //timestamp for when the process is put into queue
    //we'll queue the activeProcess now.

    sprintf(logmsg, "oss: process %ld expired at: %ld:%ld\n", pcb->userPID, shm->sysTime.sec, shm->sysTime.ns);
    writeLog( logmsg );
    queueProcess( pcb );
    activePCB = NULL;
}

void handleUnblock(){
    char logmsg[BUF_LEN];
    PCB *tempPCB;
    int tempLocalPID;
    int i;
    printf("printing blockedQueue:\n");
    printQueue(blockedQueue);
    if( !isQueueEmpty(blockedQueue) ) {
        if( !activePCB ){
            printf("inside unblock handler. There isn't an active process. Passing time\n");
            addTime(&shm->sysTime, 5 * SECOND);
            addTime(&idleTime, 5 * SECOND);
        }
        for (i = 0; i < blockedQueue->currentCapacity; i++) {
            int j = (blockedQueue->front + i) % blockedQueue->size;
            tempLocalPID = pop(blockedQueue);      //assigned the local pid so we can pop it
            tempPCB = getPTablePCB(tempLocalPID);         //getting our PCB so we can do the operations we need
            if (receiveMsg(&message, tempPCB->userPID, getPMsgID(), false) == -1) {
                printf("oss: did not receive an unblocked message. Moving on...\n");
                //since we didn't get an unblocked message we put the PCB back into blocked queue
                push(blockedQueue, tempLocalPID);
            } else if (strcmp(message.msg, "UNBLOCK") == 0) {
                printf("./oss unblock received from %ld\n", (long) tempPCB->userPID);
                //we've received the blocked process unblocking itself.
                //We want to put the process into a regular queue
                //messaging the oss to unblock them.
                //we'll be using a popIndex.
                //we'll be poping the blockedQueue based on the local pid.

                //we want to copy the exact time it is going to get queued into our system.
                printf("tempPCB priority: %d\n", tempPCB->priority );
                sprintf(logmsg, "oss: process %ld unblocked at: %ld:%ld\n", tempPCB->userPID, shm->sysTime.sec, shm->sysTime.ns);
                writeLog( logmsg );
                copyTime(&shm->sysTime, &tempPCB->queuedTime);
                if (tempPCB->priority == 0) {
                    push(highQueue, tempLocalPID);
                } else if (tempPCB->priority == 1){
                    push(lowQueue, tempLocalPID);
                }
                if (!activePCB){
                    scheduleProcess();
                }
            }
        }
    }
}

void queueProcess( PCB *pcb ){
    //we'll make a queue process here
    //two cases activePCB is either priority 0 or 1
    if( pcb->priority == 0 ){
        //priority is 0
        printf("pushing localPID: %d into highQueue\n", pcb->localPID);
        push( highQueue, pcb->localPID );
        printf("printing highQueue:\n");
        printQueue( highQueue );
        copyTime( &shm->sysTime, &pcb->queuedTime );
    } else {
        //priority is 1
        printf("pushing localPID: %d into lowQueue\n", pcb->localPID);
        push( lowQueue, pcb->localPID );
        printf("printing lowQueue:\n");
        printQueue( lowQueue );
        copyTime( &shm->sysTime, &pcb->queuedTime );
    }
}

void scheduleProcess(){
    if( activePCB ){
        printf("There is currently an active Process\n");
        return;
    }

    char logmsg[BUF_LEN];
    Time queuedWait1;                                       //time waiting in queue by this high priority process
    Time queuedWait2;                                       //time waiting in queue by this low priority process

    int highlocalpid;
    int lowlocalpid;
    int activeLocalPID;
    PCB *pcb1 = NULL;
    PCB *pcb2 = NULL;

    if( isQueueEmpty(highQueue) && isQueueEmpty(lowQueue) ){
        printf("both priority queues are empty currently\n");
        return;
    }

    if( !isQueueEmpty(highQueue) ) {
        highlocalpid = findFrontPID(highQueue);
        printf("found highlocalpid: %d\n", highlocalpid);
        pcb1 = getPTablePCB(highlocalpid);
        printf("got process: %ld\n", (long) pcb1->userPID);
        queuedWait1.sec = shm->sysTime.sec - pcb1->queuedTime.sec;
        queuedWait1.ns = shm->sysTime.ns - pcb1->queuedTime.ns;
        if( isQueueEmpty( lowQueue ) ){
            //popping high queue since low queue is empty.
            printf("popping high queue\n");
            activeLocalPID = pop( highQueue );
            printf("process table index: %d grabbed\n", activeLocalPID);
            activePCB = pcb1;
            printf("loading process %ld to be activeProcess\n", (long)activePCB->userPID);
            addTime( &activePCB->totalWait, queuedWait1.sec * SECOND );
            addTime( &activePCB->totalWait, queuedWait1.ns);
            makeActiveProcess( activePCB );
            return;
        }
    }

    if( !isQueueEmpty(lowQueue) ) {
        lowlocalpid = findFrontPID(lowQueue);
        printf("found lowlocalpid: %d\n", lowlocalpid);
        pcb2 = getPTablePCB(lowlocalpid);
        printf("got process: %ld\n", (long) pcb2->userPID);
        queuedWait2.sec = shm->sysTime.sec - pcb2->queuedTime.sec;
        queuedWait2.ns = shm->sysTime.ns - pcb2->queuedTime.ns;
        if ( isQueueEmpty( highQueue ) ){
            //popping low queue since high queue is empty.
            printf("popping low queue\n");
            activeLocalPID = pop( lowQueue );
            printf("process table index: %d grabbed\n", activeLocalPID);
            activePCB = pcb2;
            printf("loading process %ld to be activeProcess\n", (long)activePCB->userPID);
            addTime( &activePCB->totalWait, queuedWait2.sec * SECOND );
            addTime( &activePCB->totalWait, queuedWait2.ns);
            makeActiveProcess( activePCB );
            return;
        }
    }


    //we will run a check on the next processes from each queue (high and low)
    //scheduler doesn't prioritize on priority alone. We also consider other things:
    // 1. age of the process
    // 2. the time the process has been waiting for since last CPU burst.

    // we'll get the age of the process by the time it has been in the system.

    // we'll get the time waiting by taking current system time and subtracting from the time stamp of the last burst.
    // basically: ( shm->sysTime - pcb->queuedTime )

    // normally we do give priority to high priority queue.
    // However, we do want consider cases where a low priority process has waited too long.

    // if both the age of the process is older AND the waiting time is greater than higher priority, then
    // we shall give low priority process the opportunity.

    //we've got both queues and will make the comparisons to give us an activePCB from a queue
    if( queuedWait2.sec > queuedWait1.sec ) {
        // a whole second is a big difference, we shall give low Queue a priority this case.
        activeLocalPID = pop( lowQueue );
        activePCB = getPTablePCB( activeLocalPID );
        addTime( &activePCB->totalWait, queuedWait2.sec * SECOND );
        addTime( &activePCB->totalWait, queuedWait2.ns);
    } else if ( pcb2->totalSysTime.sec > pcb1->totalSysTime.sec ){
        // we'll take age to consideration. Again 1 second is a big difference, so we shall give low Queue
        // priority again.
        activeLocalPID = pop( lowQueue );
        activePCB = pcb2;
        addTime( &activePCB->totalWait, queuedWait2.sec * SECOND );
        addTime( &activePCB->totalWait, queuedWait2.ns);
    } else if ( queuedWait2.ns > QUANTUM * 3 ) {
        // we'll consider giving lower queue the priority if its wait time in nano seconds is greater than
        // 3 timeslices.
        activeLocalPID = pop( lowQueue );
        activePCB = pcb2;
        addTime( &activePCB->totalWait, queuedWait2.sec * SECOND );
        addTime( &activePCB->totalWait, queuedWait2.ns);
    } else {
        activeLocalPID = pop( highQueue );
        activePCB = pcb1;
        addTime( &activePCB->totalWait, queuedWait1.sec * SECOND );
        addTime( &activePCB->totalWait, queuedWait1.ns);
    }
    makeActiveProcess( activePCB );
}

void passTime(){
    long timePassed;
    timePassed = ( SECOND + ((rand() % 1000 ) + 1) );
    addTime( &shm->sysTime, timePassed );
    if( !activePCB ){
        addTime( &idleTime, timePassed );
    }
}

void makeActiveProcess( PCB *pcb ){
    char logmsg[BUF_LEN];
    clearTime( &pcb->timeLimit );
    if( pcb->priority == 1 ) {
        addTime(&pcb->timeLimit, QUANTUM * 2);
    } else {
        addTime(&pcb->timeLimit, QUANTUM);
    }
    sprintf(logmsg, "oss: process %ld dispatched at: %ld:%ld\n", pcb->userPID, shm->sysTime.sec, shm->sysTime.ns);
    writeLog( logmsg );
    if (sendMsg(&message, "START", pcb->userPID, getCMsgID(), false) == -1) {
        printf("./oss: did not send a message\n");
    } else {
        //message sent!
        printf("./oss %ld: START msg sent to %ld\n", (long) getpid(), (long) pcb->userPID);
    }
}

void writeLog(const char* logmsg) {
    /* logmsg in the format:
    *   Time PID Iteration# of NumberOfIterations
    */
    //The writeLog will write the logmsg to the .log file
    if (totalLines <= 10000) {
        //Now writing to file
        fprintf(saveFile, "%s\n", logmsg);
        //closing file
//        printf("File saved to: %s\n", logName);
        //return 0 if successful
        return;
    }
}