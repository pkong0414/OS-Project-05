

#ifndef SIGNALHANDLER_H
#define SIGNALHANDLER_H


// SIGNAL HANDLERS
void myTimeOutHandler( int s );                    //This is our signal handler for timeouts
void myKillSignalHandler( int s );                 //This is our signal handler for interrupts
int setupUserInterrupt( void );
int setupinterrupt( void );
int setupitimer( int tValue );

#endif