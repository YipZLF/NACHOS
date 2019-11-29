// synchconsole.h 
// 	Data structures to export a synchronous interface to the raw 
//	console.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef SYNCHCONSOLE_H
#define SYNCHCONSOLE_H

#include "console.h"
#include "synch.h"


class SynchConsole {
  public:
    SynchConsole(char *readFile, char *writeFile, VoidFunctionPtr readAvail, 
	VoidFunctionPtr writeDone, int callArg); 

    ~SynchConsole();			
    
    void PutChar(char ch);

    char GetChar();
    
    void RequestDone();	

  private:
    Console *console;		  		// Raw disk device
    Semaphore *semaphore; 		// To synchronize requesting thread 
					// with the interrupt handler
    Lock *lock;		  		// Only one read/write request
					// can be sent to the disk at a time
};

#endif // SYNCHDISK_H
