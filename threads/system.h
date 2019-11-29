/*
 * @Description: In User Settings Edit
 * @Author: your name
 * @Date: 2019-09-11 23:49:35
 * @LastEditTime: 2019-09-28 11:47:14
 * @LastEditors: Please set LastEditors
 */
// system.h 
//	All global variables used in Nachos are defined here.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef SYSTEM_H
#define SYSTEM_H

#include "copyright.h"
#include "utility.h"
#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "stats.h"
#include "timer.h"
#include "synch.h"

// Initialization and cleanup routines
extern void Initialize(int argc, char **argv); 	// Initialization,
						// called before anything else
extern void Cleanup();				// Cleanup, called when
						// Nachos is done.

// The following array defines the array for threads info
// tidmap_array[tid] -> the address of the tid-th thread
extern Thread * tidmap_array[MAX_THREAD_NUM];
extern int current_max_tid ;  			// current maximum tid
extern Thread *currentThread;			// the thread holding the CPU
extern Thread *threadToBeDestroyed;  		// the thread that just finished
extern Scheduler *scheduler;			// the ready list
extern Interrupt *interrupt;			// interrupt status
extern Statistics *stats;			// performance metrics
extern Timer *timer;				// the hardware alarm clock

#ifdef USER_PROGRAM
#include "machine.h"
extern Machine* machine;	// user program memory and registers

#ifdef TLB_MISS_LRU
static void
TLBMissInterruptHandler(int dummy){
    DEBUG('s',"----------------Tick! Timer Interrupt!-----------------\n");
    //printf("----------------Tick! Timer Interrupt!-----------------\n");

    for(int i = 0 ;i < TLBSize;++i){
        machine->lru_counter[i]++;
    }
}

#endif

#endif

#ifdef FILESYS_NEEDED 		// FILESYS or FILESYS_STUB 
#include "filesys.h"
extern FileSystem  *fileSystem;
#endif

#ifdef FILESYS
#include "synchdisk.h"
extern SynchDisk   *synchDisk;
extern SynchFileTable *synchFileTable;
#endif

#ifdef NETWORK
#include "post.h"
extern PostOffice* postOffice;
#endif

#endif // SYSTEM_H
