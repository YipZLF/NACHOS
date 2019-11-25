
// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"

// testnum is set in main.cc
int testnum;


//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	printf("*** thread %d looped %d times\n", which, num);
        currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1\n");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, (void*)1);
    SimpleThread(0);
}
//----------------------------------------------------------------------
// ThreadTest_TS
//  test threadstatus
//----------------------------------------------------------------------

void ThreadStatus(int arg){
    printf("Name\tUID\tTID\tStatus\n");
    for(int i = 0; i < MAX_THREAD_NUM;++i){
        if(tidmap_array[i] == NULL)
            continue;
        char status[10],name[10];
        char* _name=tidmap_array[i]->getName();
        int uid = tidmap_array[i]->getUID();
        int tid = tidmap_array[i]->getTID();
        strcpy(name, _name);
        switch(tidmap_array[i]->getStatus()){
            case JUST_CREATED: strcpy(status, "RUNNING" ); break;
            case RUNNING: strcpy(status, "RUNNING" ); break;
            case READY: strcpy(status, "RUNNING" ); break;
            case BLOCKED: strcpy(status, "RUNNING" ); break;
            default: 
                DEBUG('t',"Thread %d \"%s\" status error: %d",
                      tid,name,tidmap_array[i]->getStatus());
                return ;
        }
        printf("%s\t%d\t%d\t%s\n",name,uid,tid,status);
    }
}

void ThreadTest_TS(){
    DEBUG('t', "Entering ThreadTest_TS\n");
    Thread *t = new Thread("TS",0,0);
    t->Fork(ThreadStatus,(void*)1);
}
//----------------------------------------------------------------------
// ThreadTest_Prio
//  test priority
//----------------------------------------------------------------------
void SimpleThread_prio(int which){
    int num;
    for (num = 0; num < 3; num++) {
	    printf("*** thread %d %s looped %d times, prio = %d\n", currentThread->getTID(),
                currentThread->getName(), num,currentThread->getPriority());
        currentThread->Yield();
    }
}

void ForkThread(int t){
    ((Thread *)t)->Fork(SimpleThread_prio,(void*)1);
}

void ThreadTest_Prio(){
    DEBUG('t', "Entering ThreadTest_prio");
    Thread *t0 = new Thread("test_prio0",1,0);
    Thread *t1 = new Thread("test_prio1",1,1);
    Thread *t2 = new Thread("test_prio2",1,2);
    Thread *t3 = new Thread("test_prio3",1,3);

    interrupt->Schedule(ForkThread,(int)t0,50,ConsoleWriteInt);
    
    t1->Fork(SimpleThread_prio,(void*)1);
    t3->Fork(SimpleThread_prio,(void*)1);
    t2->Fork(SimpleThread_prio,(void*)1);
}
//---------------------------------------------------
// Multiqueue test
//---------------------------------------------------
void
SimpleThread_mq(int which)
{
    int num;
    
    for (num = 0; num < which; num++) {
	    printf("*** thread %d %s looped %d times, prio = %d, started at ticks %d\n", currentThread->getTID(),
                currentThread->getName(), num,currentThread->getPriority(),currentThread->getStartTime());
        currentThread->Yield();
    }
}

void ThreadTest_multiqueue(){
    DEBUG('t', "Entering ThreadTest_multiqueue");
    Thread *t0 = new Thread("test_prio0",1,0);
    Thread *t1 = new Thread("test_prio1",1,0);
    Thread *t2 = new Thread("test_prio2",1,0);

    t0->Fork(SimpleThread_mq,(void*)5);
    t1->Fork(SimpleThread_mq,(void*)10);
    t2->Fork(SimpleThread_mq,(void*)13);
}


//----------------------------------------------------------------------
// SyncTest
// 	Producer-consumer problem.
//  NOTICE!! There is a problem that if non-deprivative scheduler is used,
//           there might be starving problem. WATCHDOG is needed. Or periodically 
//           yielding the current thread to change the order of ReadyToRun queue.
//----------------------------------------------------------------------
#define SYNC_BUFFER_SIZE 3
Lock mutex("mutex");
Condition empty("empty"),full("full");
int buffer = 0;
void Producer(int arg){
    while(true){
        interrupt->OneTick();
        mutex.Acquire();
        while(buffer == SYNC_BUFFER_SIZE){
            DEBUG('s',"Producer %d waits for empty. Now buffersize: %d\n",arg,buffer);
            empty.Wait(&mutex);
        }
        DEBUG('s',"Producer %d inserted data. Now buffersize: %d\n",arg,buffer);
        buffer += 1;
        DEBUG('s',"Producer %d signal full. Now buffersize: %d\n",arg,buffer);
        full.Signal(&mutex);
        mutex.Release();
    }
}
void Consumer(int arg){
    while(true){
        interrupt->OneTick();
        mutex.Acquire();
        while(buffer == 0){
            DEBUG('s',"Consumer %d waits for full. Now buffersize: %d\n",arg,buffer);
            full.Wait(&mutex);
        }
        DEBUG('s',"Consumer %d consumers data. Now buffersize: %d\n",arg,buffer);
        buffer -= 1;
        DEBUG('s',"Consumer %d signal empty. Now buffersize: %d\n",arg,buffer);
        empty.Signal(&mutex);
        mutex.Release();
    }
}
static void
SyncTimerInterruptHandler(int dummy)
{
    DEBUG('s',"----------------Tick! Timer Interrupt!-----------------\n");
    //printf("----------------Tick! Timer Interrupt!-----------------\n");
    
    if (interrupt->getStatus() != IdleMode)
	    interrupt->YieldOnReturn();
}
void
SimpleThread_oneTick(int which)
{
    int num;
    
    for (num = 0; num < 50; num++) {
	    printf("*** thread %s looped %d times\n", currentThread->getName(), num);
        interrupt->OneTick();
    }
}
void Producer_consumer_test(){
    timer = new Timer(SyncTimerInterruptHandler, 0, false);
    Thread *pro1 = new Thread("producer 1",1);
    Thread *pro2 = new Thread("producer 2",1);
    Thread *con1 = new Thread("consumer 1",1);
    Thread *con2 = new Thread("consumer 2",1);

    pro1->Fork(Producer,(void*)1);
    pro2->Fork(Producer,(void*)2);
    con1->Fork(Consumer,(void*)1);
    con2->Fork(Consumer,(void*)2);
}

//----------------------------------------------------------------------
// SyncTest
// 	Reader-writer problem.
//  NOTICE!! There is a problem that if non-deprivative scheduler is used,
//           there might be starving problem. WATCHDOG is needed. Or periodically 
//           yielding the current thread to change the order of ReadyToRun queue.
// QUESTION: READER 2 seems to starve
//----------------------------------------------------------------------
/*
Semaphore rc_sem("rc_sem",1),write("write",1);
int rc = 0;
void Writer(int arg){
        DEBUG('s',"Writer %d. Now reader_count: %d\n",arg,rc);
    while(true){
        interrupt->OneTick();

        write.P();
        DEBUG('s',"Writer %d writes. Now reader_count: %d\n",arg,rc);
        
        write.V();
    }
}
void Reader(int arg){
        DEBUG('s',"Reader %d. Now rc: %d\n",arg,rc);
    while(true){
        interrupt->OneTick();
        
        rc_sem.P();
        rc+=1;
        if(rc == 1) {
            write.P();
            DEBUG('s',"Reader %d enters\n",arg);
        }
        rc_sem.V();
        
        DEBUG('s',"Reader %d reads. Now reader_count: %d\n",arg,rc);
        
        rc_sem.P();
        rc-=1;
        DEBUG('s',"Reader %d leaves\n",arg);
        if(rc == 0) write.V();
        rc_sem.V();
    }
}


void Reader_writer_test(){
    timer = new Timer(SyncTimerInterruptHandler, 0, true);
    Thread *wr1 = new Thread("writer 1",1);
    Thread *wr2 = new Thread("writer 2",1);
    Thread *re1 = new Thread("reader 1",1);
    Thread *re2 = new Thread("reader 2",1);

    wr1->Fork(Writer,(void*)1);
    wr2->Fork(Writer,(void*)2);
    re1->Fork(Reader,(void*)1);
    re2->Fork(Reader,(void*)2);
}

*/
//---------------
// User Program TLB miss lru
//---------------

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
        case 1:
            ThreadTest1(); break;
        case 2:
            ThreadTest_TS(); break;
        case 3:
            ThreadTest_Prio(); break;
        case 4:
            ThreadTest_multiqueue(); break;
        case 5:
            Producer_consumer_test(); break;
     //   case 6:
     //       Reader_writer_test(); break;
        default:
            printf("No test specified.\n"); break;
    }
}


