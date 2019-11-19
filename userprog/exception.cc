// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include<string.h>

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void PageFaultDiskIntHandler(int thread_ptr){
    Thread* thread_to_wake = (Thread*)thread_ptr;
    DEBUG('m',"Incoming disk interrupt to wake up thread %d \"%s\"\n",
            thread_to_wake->getTID(),thread_to_wake->getName());
    scheduler->ReadyToRun( thread_to_wake);
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if (which == SyscallException) {
        switch(type){
            case SC_Halt:{
                DEBUG('a', "Shutdown, initiated by user program.\n");
                interrupt->Halt(); break;
                }
            case SC_Exit:{
                DEBUG('e', "Exit from userprog. with %d \n",machine->ReadRegister(4));
                int pt_size = machine->pageTableSize;
                for(int i = 0 ;i < pt_size;++i){
                    int cnt = machine->pageTable[i].physicalPage * PageSize / BytesPerBit;
                    MemoryBitmap->Clear(cnt);
                    MemoryBitmap->Clear(cnt+1);
                }
                currentThread->Finish();
                break;
            }
            default:{
                printf("Unexpected user mode exception %d %d\n", which, type);break;
            }
        }
    } else if(which == PageFaultException){
        TranslationEntry * tlb =  machine->tlb;
        if(tlb){
            unsigned int bad_v_addr = machine->ReadRegister(BadVAddrReg);
            unsigned int vpn = bad_v_addr / PageSize;
            if (!machine->pageTable[vpn].valid) { // page fault
			    DEBUG('m', "Virtual page fault, addr: # %d  \n", bad_v_addr);
                //dealing with page fault
            }else{
                DEBUG('m', "TLB miss, addr:# %d  \n", bad_v_addr);
                machine->TLBExceptionHandler(vpn);
            }
		}
        else{ // no tlb, page fault
            DEBUG('m',"INTO PAGE FAULT HANDLER!\n");
            unsigned int bad_v_addr = machine->ReadRegister(BadVAddrReg);
            unsigned int vpn = bad_v_addr / PageSize;
            int ppn_to_swap = machine->oldest_main_mem_page;
            int page_to_tid = machine->phys_page_to_thread[ppn_to_swap];
            TranslationEntry* rev_map_tle = (TranslationEntry*)machine->reverse_mapping_table[ppn_to_swap];

            DEBUG('m',"Decide to swap page #%d\n", ppn_to_swap);

            if(rev_map_tle){// allocated
                if(rev_map_tle->dirty){
                    memcpy( & myDisk[page_to_tid * DiskSizePerThread + rev_map_tle->virtualPage * PageSize],
                            & machine->mainMemory[ppn_to_swap * PageSize], PageSize); 
                }
                rev_map_tle->valid = FALSE;
                DEBUG('m',"-- the old page is allocated to thread %d, DIRTY=%d\n",page_to_tid,rev_map_tle->dirty);
            }
            // not allocated
            machine->reverse_mapping_table[ppn_to_swap] = (unsigned int)&machine->pageTable[vpn];
            machine->phys_page_to_thread[ppn_to_swap] = currentThread->getTID();
            machine->oldest_main_mem_page += 1;
            machine->oldest_main_mem_page %= NumPhysPages;

            DEBUG('m',"Now allocate phys page #%d to thread %d\n",ppn_to_swap,currentThread->getTID());

            machine->pageTable[vpn].physicalPage = ppn_to_swap;

            // For simulation purpose:
            interrupt->Schedule(PageFaultDiskIntHandler,(int)currentThread,100,DiskInt);

            currentThread->Sleep();
        }
    } else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
