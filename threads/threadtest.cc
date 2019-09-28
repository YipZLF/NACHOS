/*
 * @Description: In User Settings Edit
 * @Author: your name
 * @Date: 2019-09-11 23:49:35
 * @LastEditTime: 2019-09-28 15:56:28
 * @LastEditors: Please set LastEditors
 */
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
int testnum = 1;

//----------------------------------------------------------------------
// ThreadStatus
// 	Print out thread status.
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
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, (void*)1);
    SimpleThread(0);
}
//----------------------------------------------------------------------
// ThreadTest_TS
//  test threadstatus
//----------------------------------------------------------------------

void
ThreadTest_TS()
{
    DEBUG('t', "Entering ThreadTest_TS");

    Thread *t = new Thread("TS",0);

    t->Fork(ThreadStatus,(void*)1);

}

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
	ThreadTest1();
	break;
    case 2:
    ThreadTest_TS();
    default:
	printf("No test specified.\n");
	break;
    }
}


