# OS-Project-05
An OS Resource Simulator

Usage: %s [-h] [-t <seconds. DEFAULT:3 MAX:100>] [-l < logName >.log ]

-h: help.

-t: time out timer. Default is 3 seconds. Max is 100 seconds.

-l: the name of the logfile. Default is logfile. '.log' will be appended at the end
    to form the logfile.

This program simulates an OS scheduler. The program will create up to 50 processes
until it times out. The Process Table will only contain up to 18 Process Control
Blocks. Local Pids are handed out by the OSS with an array of bits.

I've added and revised functions inside queue.c. We want a rotate queue function
so we can look around for a safe way to handle deadlocks. 

blockedQueue: used to indicate we have processes that didn't receive resources

safeQueue: used to store safe states.

messageQueue: used to handle the messaging process

removeQueue is also revised so we do not remove the queue object, since we will be
wiping safeQueue in oss.c whenever we find an unsafe state.

Safe sequences will be done by looking at our blockedQueue. If potential requested
is greater than available resource, then we will have a deadlock. We will check
over all the blocked processes and by finding one that doesn't exceed we will have
a safe sequence and that will let us simulate the granted resource.

<**** Revised safety sequence ****>

Initially I put things into a blockedQueue. Unfortunately it didn't quite work out.
I'm revising it so that I will look at our resources first before we actually,
give it the go. If there is a deadlock. All processes will now be looked at.

Same policy apply: we are looking at potential request, because if all potential
request is greater than available resources, we will have a deadlock.

Problems:

I've made revision to the deadlock detection a couple of times. There's still 
lock ups, but the program is able to grant resources and terminate will release
resources accordingly.
    