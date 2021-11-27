//sharedHandler.h

/*  I just want to make this file to simplify our main program.
 *  Aim is to have this handle all the shared Memory as well as Semaphores (if we are using them).
 *
 *  I will also be adding message queue as a way to explore that as an option for handling the scheduling.
 */

#ifndef SHAREDHANDLER_H
#define SHAREDHANDLER_H

#include <stdbool.h>
#include <stdio.h>
#include "config.h"

//macros
#define BUF_LEN 1024
#define SECOND 1000000000                                           //a second is 1000000000 nanosecs
#define MILLI 1000000                                               //a millisecond is 1000000 nanosecs
#define MACRO 1000                                                  //a macrosecond is 1000 nanosecs
#define QUANTUM 10000000                                            //our quantum base
#define REC_MAX 20                                                  //resources max

// shared memory struct
typedef struct sharedTimer{
    long sec;                                                       //seconds
    long ns;                                                        //nanoseconds
} Time;

typedef struct sharedMessage{                                       //Struct contains members for the IPC message Queue
    long type;                                                      //message type
    char msg[BUF_LEN];                                              //message itself.
} Message;

typedef struct pcb{
    //we want to have resource request coming from PCB of all processes
    int requests[ REC_MAX ];                                        //requests from user processes
    int reqMax[ REC_MAX ];                                          //max claims for the user process
    int allocate[ REC_MAX ];                                        //allocated resources for the user process.

    Time totalCpuTime;                                              //total Cpu Time in the system.
    Time totalSysTime;                                              //total time process spent in the system.
//    Time arriveTime;                                                //the timestamp from first arrived.
//    Time queuedTime;                                                //the timestamp of the time placed in queue
//    Time burstTime;                                                 //the amount of timeslice used from the last burst
//    Time timeLimit;                                                 //this is our time slice or quantum
//    Time waitTime;                                                  //timestamp of the wait, when queued up.
//    Time totalWait;                                                 //total time spent waiting.
//    Time exitTime;                                                  //the timestamp from process exiting.
    int localPID;                                                   //process's PTable id.
    pid_t userPID;                                                  //Process's user actual pid
//    int priority;                                                   //Process's priority ( either 1 or 2 )
} PCB;

typedef struct sharedMemory{                                        //Process Control Block
    Time sysTime;                                                   //the system time.
    PCB pTable[MAX_PROC];                                           //process table for the PCB

    //resource descriptors
    int available[ REC_MAX ];                                       //the system's available resources
    int shared[ REC_MAX ];                                          //the system's shared flag.
} sharedMem;

// sharedMemory functions
void initShm();                                                     //This function will initialize shared memory.
int removeShm();                                                    //This function will remove shared memory.
sharedMem *getSharedMemory();
PCB *getPTablePCB( int index );
PCB *getPTablePID( pid_t pid );
void clearAProcessTable( int index );
//int getFreePTableIndex();

// semaphore functions
void initSem();                                                     //This is going to initialize our semaphores.
int removeSem();                                                    //This is going to remove our semaphores.
void semWait(struct sembuf semW);                                   //This is for our semaphore wait.
void semSignal(struct sembuf semS);                                 //This is for our semaphore signals.
void setsembuf(struct sembuf *s, int num, int op, int flg);
int r_semop(struct sembuf *sops, int nsops);

// message queue functions
int initMsq();
int receiveMsg(Message *message, pid_t pid, int msgid, bool waiting);
int sendMsg(Message *message, char *msg, pid_t pid, int msgid, bool waiting );
int removeMsq();
int getPMsgID();                                                    //Grabbing the parent message queue ID
int getCMsgID();                                                    //Grabbing the child message queue ID

//time handling functions
void addTime(Time *time, long timeValue);
void clearTime( Time *time );
void copyTime(Time *src, Time *dest);
int compareLeftGtrEqTime( Time time1, Time time2 );
void calcPSysTime( PCB *pcb, Time sysTime );

#endif