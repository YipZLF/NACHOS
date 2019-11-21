// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    #ifdef TMP_DISK
        int tid = currentThread->getTID();
        myDiskStartAddr = tid * DiskSizePerThread;
        bzero(myDisk + myDiskStartAddr, DiskSizePerThread);
    #endif 
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

#ifdef TMP_DISK
    int a = DiskSizePerThread, b= PageSize;
    int maxPagesCnt = DiskSizePerThread/PageSize;
    ASSERT(numPages <= (DiskSizePerThread/PageSize));
#else
    ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory
#endif

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    bool bAllocPhysPage = false;
    for (i = 0; i < numPages; i++) {
        #ifdef TMP_DISK
        pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
        pageTable[i].physicalPage = -1;
        pageTable[i].valid = FALSE;
        #else
        pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
        pageTable[i].physicalPage = machine->AllocPhysPage(i,&bAllocPhysPage);
        pageTable[i].valid = bAllocPhysPage;
        bzero(&machine->mainMemory[pageTable[i].physicalPage*PageSize], PageSize);
        #endif
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
                        // a separate page, we could set its 
                        // pages to be read-only
    }
    
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment

//    bzero(machine->mainMemory, size);
    RestoreState();
// then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);

        #ifdef TMP_DISK
        DEBUG('m', "(TMP_DISK)Reading code segment into myDisk, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        executable->ReadAt(myDisk + myDiskStartAddr + noffH.code.virtualAddr, 
                            noffH.code.size, noffH.code.inFileAddr);
        /*
        char into= 0;
        for(int i = 0 ;i < noffH.code.size; ++i){
            //MemoryBitmap->Mark( into >> 6 );//2^6 = 64= BytesPerBit;
            executable->ReadAt(&into, 1, noffH.code.inFileAddr+i);
            DEBUG('m',"FILE_CONTENT_DEBUG:VAddr:%d fileLocation:%d content: %2x\n",
                noffH.code.virtualAddr+i,noffH.code.inFileAddr+i,into);    
        }
        for(int i = 0 ;i < noffH.code.size; ++i){
            DEBUG('m',"DISK_CONTENT_DEBUG: DiskPhysAddr:%8.8x VAddr:%8.8x, content:%2x (Host addr: %x\n",
                    myDiskStartAddr + noffH.code.virtualAddr+i,noffH.code.virtualAddr+i,
                    (int)myDisk[myDiskStartAddr + noffH.code.virtualAddr+i],
                    (int)&myDisk[myDiskStartAddr + noffH.code.virtualAddr+i]);    
        }*/
        #else
        int into= 0;
        for(int i = 0 ;i < noffH.code.size; ++i){
            DEBUG('m',"exception type:%d\n",machine->Translate(noffH.code.virtualAddr+i,&into,1,false));    
            MemoryBitmap->Mark( into >> 6 );//2^6 = 64= BytesPerBit;
            executable->ReadAt(&(machine->mainMemory[into]), 1, noffH.code.inFileAddr+i);
            //DEBUG('m',"VAddr:%d PAddr:%d fileLocation:%d\n",noffH.code.virtualAddr+i,into,noffH.code.inFileAddr+i);    
        }
        #endif

    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);

        #ifdef TMP_DISK
        DEBUG('m', "(TMP_DISK)Reading data segment into myDisk, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        executable->ReadAt(myDisk + myDiskStartAddr + noffH.initData.virtualAddr,
                            noffH.initData.size, noffH.initData.inFileAddr);
        #else
        int into= 0;
        for(int i = 0 ;i <  noffH.initData.size; ++i){
            machine->Translate(noffH.initData.virtualAddr+i,&into,1,false);
            MemoryBitmap->Mark( into >> 6 );//2^6 = 64= BytesPerBit;
            executable->ReadAt(&(machine->mainMemory[into]),1, noffH.initData.inFileAddr+i);
            //DEBUG('m',"data seg :VAddr:%d PAddr:%d fileLocation:%d\n",noffH.initData.virtualAddr+i,into,noffH.initData.inFileAddr+i);    
            
        }
        #endif
    }

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{
    DEBUG('m',"Save pageTable\n");
    machine->pageTable = NULL;
    machine->pageTableSize = NULL;
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    DEBUG('m',"Initialized pageTable at %u, numPages=%d\n",(unsigned int)pageTable, numPages);
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}
