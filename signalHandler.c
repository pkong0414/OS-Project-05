//signalHandler.c

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include "config.h"
#include "sharedHandler.h"
#include "signalHandler.h"

//******************************************* SIGNAL HANDLER FUNCTIONS *******************************************
void myTimeOutHandler( int s ) {
    char timeout[] = "timing out processes.\n";
    int timeoutSize = sizeof( timeout );
    int errsave;

    errsave = errno;
    write(STDERR_FILENO, timeout, timeoutSize );
    errno = errsave;
    int i;

    //waiting for max amount of children to terminate
    for (i = 0; i <= MAX_TOTAL_PROC ; ++i) {
        wait(NULL);
    }

    //detaching shared memory
    if (removeShm() == -1) {
        perror("failed to destroy shared memory segment\n");
        exit(0);
    } else {
        printf("destroyed shared memory segment\n");
    }

//    if(removeSem() == -1) {
//        perror("failed to remove semaphore\n");
//        exit(0);
//    } else {
//        printf("removed semaphore\n");
//    }

    if(removeMsq() == -1) {
        perror("failed to remove message queue\n");
        exit(0);
    } else {
        printf("removed message queue\n");
    }
    exit(0);
}

void myKillSignalHandler( int s ){
    char timeout[] = "caught ctrl+c, ending processes.\n";
    int timeoutSize = sizeof( timeout );
    int errsave;

    errsave = errno;
    write(STDERR_FILENO, timeout, timeoutSize );
    errno = errsave;
    int i;

    //waiting for max amount of children to terminate
    for (i = 0; i <= MAX_TOTAL_PROC; ++i) {
        wait(NULL);
    }
    //detaching shared memory
    if (removeShm() == -1) {
        perror("failed to destroy shared memory segment\n");
        exit(0);
    } else {
        printf("destroyed shared memory segment\n");
    }

//    if(removeSem() == -1) {
//        perror("failed to remove semaphore\n");
//        exit(0);
//    } else {
//        printf("removed semaphore\n");
//    }

    if(removeMsq() == -1) {
        perror("failed to remove message queue\n");
        exit(0);
    } else {
        printf("removed message queue\n");
    }
    exit(0);
}

int setupUserInterrupt( void ){
    struct sigaction act;
    act.sa_handler = myKillSignalHandler;
    act.sa_flags = 0;
    return (sigemptyset(&act.sa_mask) || sigaction(SIGINT, &act, NULL));
}

int setupinterrupt( void ){
    struct sigaction act;
    act.sa_handler = myTimeOutHandler;
    act.sa_flags = 0;
    return (sigemptyset(&act.sa_mask) || sigaction(SIGALRM, &act, NULL));
}

int setupitimer(int tValue) {
    struct itimerval value;
    value.it_interval.tv_sec = tValue;
    value.it_interval.tv_usec = 0;
    value.it_value = value.it_interval;
    return (setitimer(ITIMER_REAL, &value, NULL));
}