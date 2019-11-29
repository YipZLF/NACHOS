// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) { 			// semaphore not available
	queue->Append((void *)currentThread);	// so go to sleep
	currentThread->Sleep();
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if (thread != NULL)	   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
    value++;
    (void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
/*
Lock:
    维护一个信号量c，初始值为1，队列为空。lock初始值为Free
    acquire：
        如果lock为free，那么可以进入{
            lock = busy;
        }
        P(c)；
    Release:
        if(isHeldByCurrentThread()){
            lock = free;
            V(c);
        }else{
            throw exception
        }

*/
Lock::Lock(char* debugName,int initialValue):lock_thread(NULL) {
    name = debugName;
    lock_sem = new Semaphore(debugName,initialValue);
}
Lock::~Lock() {
    delete lock_sem;
}
void Lock::Acquire() {
    DEBUG('f',"Acquired lock.\n");
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    lock_sem->P();
    lock_thread = currentThread;
    (void) interrupt->SetLevel(oldLevel);
}
void Lock::Release() {
    DEBUG('f',"Released lock.\n");
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    ASSERT(lock_thread == currentThread);
    lock_sem->V();
    lock_thread = NULL;
    (void) interrupt->SetLevel(oldLevel);
}
bool Lock::isHeldByCurrentThread(){return (lock_thread==currentThread);}


/*
Condition:
    维护一个信号量c，初始值为1，队列为空。lock初始值为Free
    wait(lock):
        *turn off interrupt;
        lock.release();
        queue.append(currentThread);
        currentThread.Sleep();
        lock.acquire();
        *turn on interrupt;
    Signal(lock):
        
        *turn off interrupt;
        ASSERT(lock.isHeldByCurrentThread());

        *turn on interrupt;

*/
Condition::Condition(char* debugName) {
    name = debugName;
    condition_queue = new List;        
}
Condition::~Condition() {
    delete condition_queue;
}
void Condition::Wait(Lock* conditionLock) {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    conditionLock->Release();
    condition_queue->Append(currentThread);
    currentThread->Sleep();
    conditionLock->Acquire();
    (void) interrupt->SetLevel(oldLevel);
}
void Condition::Signal(Lock* conditionLock) {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    Thread *next_thread = NULL;
    if(!condition_queue->IsEmpty()){
        next_thread = (Thread*) condition_queue->Remove();
        scheduler->ReadyToRun(next_thread);
    }
    (void) interrupt->SetLevel(oldLevel);
}
void Condition::Broadcast(Lock* conditionLock) {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    Thread *next_thread = NULL;
    while(!condition_queue->IsEmpty()){
        next_thread = (Thread*) condition_queue->Remove();
        scheduler->ReadyToRun(next_thread);
    }
    (void) interrupt->SetLevel(oldLevel);
}
