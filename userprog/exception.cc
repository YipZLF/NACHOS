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
    scheduler->ReadyToRun(thread_to_wake);
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    if (which == SyscallException) {
        switch(type){
                        case SC_Halt:{
                DEBUG('a', "Shutdown, initiated by user program.\n");
                interrupt->Halt(); break;
                }
            case SC_Exit:{
                DEBUG('e', "Exit from userprog. with %d \n",machine->ReadRegister(4));
                printf("Exit with %d\n",machine->ReadRegister(4));
                int pt_size = machine->pageTableSize;
                /*for(int i = 0 ;i < pt_size;++i){
                    int cnt = machine->pageTable[i].physicalPage * PageSize / BytesPerBit;
                    MemoryBitmap->Clear(cnt);
                    MemoryBitmap->Clear(cnt+1);
                }*/
                currentThread->Finish();
                break;
            }
            case SC_Create:{
                DEBUG('s',"SysCall: Called Create.\n");
                char* name = (char*)machine->ReadRegister(4);
                //DEBUG('s'," - Filename: \"%s \"\n",name);
                fileSystem->Create(name,0);
                break;
            }
            case SC_Open:{
                DEBUG('s',"SysCall: Called Open.\n");
                char* name = (char*)machine->ReadRegister(4);
                // need better solution to manage the openfile list
                OpenFileId id = cur_openfile_cnt;
                OpenFile *file = fileSystem->Open(name);
                OpenFileList[id] = file;

                // need better solution to manage the openfile list
                cur_openfile_cnt++;
                machine->WriteRegister(2,(int)id);
                break;
            }
            case SC_Close:{
                DEBUG('s',"SysCall: Called Close.\n");
                OpenFileId id = (OpenFileId)machine->ReadRegister(4);
                if (id ==0 || id ==1){
                    DEBUG('s'," - Trying to close console.\n");
                    interrupt->Halt();
                }
                delete OpenFileList[id];
                OpenFileList[id] = NULL;
                break;
            }
            case SC_Write:{
                DEBUG('s',"SysCall: Called Write.\n");
                char * buffer = (char*)machine->ReadRegister(4);
                int size = (int)machine->ReadRegister(5);
                OpenFileId id = (OpenFileId)machine->ReadRegister(6);
                ASSERT(id!=ConsoleInput);
                if(id == ConsoleOutput){
                    DEBUG('s'," - to console write.\n");
                }else{
                    OpenFile *file = OpenFileList[id];
                    file->Write(buffer,size);
                }
                break;
            }
            case SC_Read:{
                DEBUG('s',"SysCall: Called Read.\n");
                char * buffer = (char*)machine->ReadRegister(4);
                int size = (int)machine->ReadRegister(5);
                OpenFileId id = (OpenFileId)machine->ReadRegister(6);
                ASSERT(id!=ConsoleOutput);
                if(id == ConsoleInput){
                    DEBUG('s'," - from console read.\n");
                }else{
                    OpenFile *file = OpenFileList[id];
                    file->Read(buffer,size);
                }break;
            }
            case SC_Exec:{
                DEBUG('s',"SysCall: Called Exec.\n");
                interrupt->Halt();break;
            }
            case SC_Yield:{
                DEBUG('s',"SysCall: Called Yield\n");
                interrupt->Halt();break;
            }
            case SC_Join:{
                DEBUG('s',"SysCall: Called Join.\n");
                interrupt->Halt();break;
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
            machine->PageFaultHandler();
            // For simulation purpose:

            /*
            DEBUG('m',"**CHECK MEM CONTENT:\n");
            for(int i = 0 ;i < PageSize;++i){
                DEBUG('m',"* Mem: %8.8x: %2x \t Disk: %8.8x, %2x,(Host Addr:%x)\n",
                        ppn_to_swap * PageSize+i,machine->mainMemory[ppn_to_swap * PageSize+i],
                        currentThread->getTID() * DiskSizePerThread + vpn * PageSize + i,
                        myDisk[currentThread->getTID() * DiskSizePerThread + vpn * PageSize+i],
                        (int)&myDisk[currentThread->getTID() * DiskSizePerThread + vpn * PageSize+i]);
            }
            */
            interrupt->Schedule(PageFaultDiskIntHandler,(int)currentThread,150,DiskInt);

            currentThread->Sleep();

        }
    } else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }

    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}
