# OS-Project-04
An OS Resource Simulator

Usage: %s [-h] [-t <seconds. DEFAULT:3 MAX:100>] [-l < logName >.log ]

-h: help.

-t: time out timer. Default is 3 seconds. Max is 100 seconds.

-l: the name of the logfile. Default is logfile. '.log' will be appended at the end
    to form the logfile.

This program simulates an OS scheduler. The program will create up to 50 processes
until it times out. The Process Table will only contain up to 18 Process Control
Blocks. Local Pids are handed out by the OSS with an array of bits.

Problems:
    Majority of the time had been spent working on the flow of the 
blocked processes. For some reason the program thinks there's an activePCB 
when the activePCB has been in NULL, this all happens after a process gets unblocked.