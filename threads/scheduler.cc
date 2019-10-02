/*
 * @Description: In User Settings Edit
 * @Author: your name
 * @Date: 2019-09-18 14:52:34
 * @LastEditTime: 2019-10-02 15:40:27
 * @LastEditors: Please set LastEditors
 */
// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "scheduler.h"
#include "memory.h"
#include "system.h"
Thread * tidmap_array[MAX_THREAD_NUM];
int current_max_tid ; 
extern const int timeslice_len[PRIORITY_LEVEL_NUM]={10,20,30,40,50};
//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads to empty.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    memset(tidmap_array,NULL,sizeof(tidmap_array));
    current_max_tid = -1;
    for(int i = 0 ;i < PRIORITY_LEVEL_NUM; ++i)
        readyMultiqueue[i] = new List; 
    readyList = readyMultiqueue[HIGHEST_PRIORITY];
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    for(int i = 0 ;i < PRIORITY_LEVEL_NUM; ++i)
        delete readyMultiqueue[i];  
} 

//----------------------------------------------------------------------
// Scheduler::AssignTID
// 	Assign tid for a new thread
//----------------------------------------------------------------------
int Scheduler::AssignTID()
{
    int next_tid = current_max_tid+1;
    for(int i = 0; i < MAX_THREAD_NUM; ++i, 
        next_tid = (next_tid+1) % MAX_THREAD_NUM){
        if(tidmap_array[next_tid] == NULL){
            return next_tid;
        }
    }
    return -1;
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    
    DEBUG('t', "Putting thread %s on ready list.\n", thread->getName());
    ThreadStatus old_status = thread->getStatus();
    thread->setStatus(READY);
    int used_ticks = thread->getUsedTicks(stats->totalTicks);
    int prio = thread->getPriority();
    int new_prio = prio;
    DEBUG('t',"Thread %d %s used ticks %d started at %d \n Used slice %d, Slice len %d \n",
        currentThread->getTID(),currentThread->getName(),used_ticks, currentThread->getStartTime(),
        USED_TIME_SLICE(used_ticks,prio) ,timeslice_len[prio]);

    if( old_status != JUST_CREATED && USED_TIME_SLICE(used_ticks,prio) >= MAX_TIME_SLICE_CNT){
        new_prio = (prio+1 > LOWEST_PRIORITY)? (LOWEST_PRIORITY): (prio +1);
        thread->setPriority(new_prio);
        thread->setUsedTicks(0);
    }

    DEBUG('t', "!!!!!Putting thread %s on ready list %d\n", thread->getName(),new_prio);
    readyMultiqueue[new_prio]->Append((void*)thread);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    int used_ticks = currentThread->getUsedTicks(stats->totalTicks);
    int prio = currentThread->getPriority();
    ThreadStatus status = currentThread->getStatus();
    if(USED_TIME_SLICE(used_ticks,prio) < MAX_TIME_SLICE_CNT && status==RUNNING){
       //     printf("Not switching!! used time slices: %d max :%d\n",USED_TIME_SLICE(used_ticks,prio) , MAX_TIME_SLICE_CNT);
        return currentThread;
    }
    
    List *next_readyList = NULL;
    int mark =0;
    for(int i = HIGHEST_PRIORITY; i<=LOWEST_PRIORITY;++i){
        if(!readyMultiqueue[i]->IsEmpty()){
            next_readyList = readyMultiqueue[i];
            mark = i;
            break;
        }
    }


    if(next_readyList==NULL) return NULL;
    else{
       // printf("used ticks %d\n",used_ticks);
       // printf("prio %d\n",prio);
        Thread * next_thread = (Thread*)next_readyList->Remove();

    DEBUG('t', "!!!!!Removing thread %s on ready list %d\n", next_thread->getName(),mark);
        readyList = next_readyList;
        return next_thread;
    }
    /*
    Thread* next_thread = (Thread *)readyList->Remove();
    if(next_thread == NULL)
        return NULL;
    else{
        int old_prio = currentThread->getPriority();
        
        int new_prio = next_thread->getPriority();
        if(new_prio < old_prio || currentThread->getStatus()!=RUNNING){ //higher priority
            return next_thread;
        }
        else{
            DEBUG('t',"failed to find next to run: \n old thread %d: %s, prio = %d \n new thread %d: %s, prio = %d \n",
                        currentThread->getTID(),currentThread->getName(),currentThread->getPriority(),
                        next_thread->getTID(),next_thread->getName(),next_thread->getPriority());
            readyList->SortedInsert((void*)next_thread,new_prio);
            return NULL;
        }
    } */
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread)
{
    Thread *oldThread = currentThread;

#ifdef USER_PROGRAM			// ignore until running user programs 
    if (currentThread->space != NULL) {	// if this thread is a user program,
        currentThread->SaveUserState(); // save the user's CPU registers
	currentThread->space->SaveState();
    }
#endif
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    currentThread = nextThread;		    // switch to the next thread
    currentThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG('t', "Switching from thread \"%s\" to thread \"%s\"\n",
	  oldThread->getName(), nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);
    if(nextThread!=oldThread)
        currentThread->setStartTime(stats->totalTicks);
    DEBUG('t', "Now in thread \"%s\" starting at tick %d\n", currentThread->getName(),currentThread->getStartTime());

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in Thread::Finish()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        int tid_to_be_destroyed = threadToBeDestroyed->getTID();
        delete threadToBeDestroyed;
	    threadToBeDestroyed = NULL;
        tidmap_array[tid_to_be_destroyed] = NULL;
    }
    
#ifdef USER_PROGRAM
    if (currentThread->space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
	currentThread->space->RestoreState();
    }
#endif
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    printf("Ready list contents:\n");
    readyList->Mapcar((VoidFunctionPtr) ThreadPrint);
}
