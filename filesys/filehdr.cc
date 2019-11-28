// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

FileHeader::FileHeader(int cur_time, unsigned int _flag){
    create_time = last_access_time = last_modified_time = cur_time;
    flag = _flag;
    numBytes = numSectors = 0;
}
//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
if(DebugIsEnabled('f')){
            DEBUG('f',"FREE MAP in ALLOCATE:\n");
            freeMap->Print();
        }
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors){
        DEBUG('f',"Filehdr::Allocate:fail because of insufficient space\n");
	    return FALSE;		// not enough space
    }

    int i = 0;
    int firstLevel = min(numSectors,NumFirstLevelDirect);
    for (; i < firstLevel; i++){
        dataSectors[i] = freeMap->Find();
        DEBUG('f',"Filehdr::Allocate: 1st level: dataSectors[%d]:%d\n",i,dataSectors[i]);
    }
    if(i<numSectors){// need secondary index
        SecondaryIndexSectors = freeMap->Find();
        DEBUG('f',"Filehdr::Allocate: 2st level: SecondaryIndexSectors:%d\n",SecondaryIndexSectors);
        SecondaryIndex * secondaryIndex = new SecondaryIndex;
        int secondLevel = min(numSectors - NumFirstLevelDirect , NumIndexDirect);
        for(i = 0;i<secondLevel;++i){
            secondaryIndex->dataSectors[i] = freeMap->Find();
            DEBUG('f',"Filehdr::Allocate: 2st level: sec->dataSectors[%d]:%d\n",i,secondaryIndex->dataSectors[i]);
        }
        synchDisk->WriteSector(SecondaryIndexSectors,(char*)secondaryIndex);
        delete secondaryIndex;

        if( (i+NumFirstLevelDirect) < numSectors){// need third level director
            ThirdLevelIndexSectors = freeMap->Find();
            DEBUG('f',"Filehdr::Allocate: 3rd level: ThirdIndexSectors:%d\n",ThirdLevelIndexSectors);
            ThirdLevelIndex * thirdLevelIndex = new ThirdLevelIndex;

            int sectors_left = numSectors - NumFirstLevelDirect - NumIndexDirect;
            ASSERT(sectors_left <= NumIndexDirect*NumIndexDirect )

            for(i = 0;i<NumIndexDirect;++i){
                
                thirdLevelIndex->SecondaryIndexSectors[i] = freeMap->Find();
                ASSERT(thirdLevelIndex->SecondaryIndexSectors[i]>0);

                SecondaryIndex * secondaryIndex = new SecondaryIndex;
                for(int j= 0; j< NumIndexDirect; ++j){
                    secondaryIndex->dataSectors[j] = freeMap->Find();
                    sectors_left--;
                    if(sectors_left==0) break;
                }
                synchDisk->WriteSector(thirdLevelIndex->SecondaryIndexSectors[i],(char*)secondaryIndex);
                delete secondaryIndex;

                if(sectors_left==0) break;
            }
            synchDisk->WriteSector(ThirdLevelIndexSectors,(char*)thirdLevelIndex);
            delete thirdLevelIndex;
        }
    }
    return TRUE;
}

int FileHeader::AppendOneSector(BitMap *freeMap){
    if (freeMap->NumClear() == 0){
        DEBUG('f',"Filehdr::AppendOneSector:fail because of insufficient space\n");
	    return -1;		// not enough space
    }


    if(0<=numSectors && numSectors <= NumFirstLevelDirect-1){
        DEBUG('f',"FileHeader:: 1 level index append\n");
        int sector = freeMap->Find();
        dataSectors[numSectors] = sector;
        numSectors++;
        return sector;
    }else if(NumFirstLevelDirect-1 < numSectors && numSectors <= NumFirstLevelDirect + NumIndexDirect){
        DEBUG('f',"FileHeader:: 2nd level index append\n");
        int idx = numSectors - NumFirstLevelDirect;
        if(idx ==0){//first secondary index
            SecondaryIndexSectors = freeMap->Find();
        }
        int sector = freeMap->Find();
        
        SecondaryIndex * secondaryIndex = new SecondaryIndex;
        synchDisk->ReadSector(SecondaryIndexSectors,(char*)secondaryIndex);
        secondaryIndex->dataSectors[idx] = sector;
        synchDisk->WriteSector(SecondaryIndexSectors,(char*)secondaryIndex);
        numSectors++;
        delete secondaryIndex;
        return sector;
    }else if(NumFirstLevelDirect + NumIndexDirect < numSectors  && 
            numSectors <=NumFirstLevelDirect + NumIndexDirect + NumIndexDirect *NumIndexDirect){
        DEBUG('f',"FileHeader:: 3rd level index append\n");
        int sectors_left = numSectors - (NumFirstLevelDirect + NumIndexDirect);
        int idx_1 = sectors_left / NumIndexDirect;
        int idx_2 = sectors_left % NumIndexDirect;
        if(idx_1 ==0 && idx_2==0){//first 3-level index
            ThirdLevelIndexSectors = freeMap->Find();
        }

        ThirdLevelIndex * thirdLevelIndex = new ThirdLevelIndex;
        SecondaryIndex * secondaryIndex = new SecondaryIndex;
        synchDisk->ReadSector(ThirdLevelIndexSectors,(char*)thirdLevelIndex);

        int sector;
        if(idx_2==0){
            thirdLevelIndex->SecondaryIndexSectors[idx_1] = freeMap->Find();
            sector = freeMap->Find();
            secondaryIndex->dataSectors[idx_2] = sector;
            synchDisk->WriteSector(ThirdLevelIndexSectors,(char*)thirdLevelIndex);
            synchDisk->WriteSector(thirdLevelIndex->SecondaryIndexSectors[idx_1],(char*)secondaryIndex);
        }else{
            synchDisk->ReadSector(thirdLevelIndex->SecondaryIndexSectors[idx_1],(char*)secondaryIndex);
            sector = freeMap->Find();
            secondaryIndex->dataSectors[idx_2] = sector;
            synchDisk->WriteSector(thirdLevelIndex->SecondaryIndexSectors[idx_1],(char*)secondaryIndex);
        }


/*
        if(idx_2 != (NumIndexDirect-1)){
            synchDisk->ReadSector(thirdLevelIndex->SecondaryIndexSectors[idx_1],(char*)secondaryIndex);
            secondaryIndex->dataSectors[idx_2] = sector;
            synchDisk->WriteSector(thirdLevelIndex->SecondaryIndexSectors[idx_1],(char*)secondaryIndex);
        }else{ // full
            ASSERT(idx_2==0);
            int new_index_sector = freeMap->Find();
            thirdLevelIndex->SecondaryIndexSectors[idx_1+1] = new_index_sector;
            secondaryIndex->dataSectors[idx_2] = sector;
            synchDisk->WriteSector(new_index_sector,(char*)secondaryIndex);
            synchDisk->WriteSector(ThirdLevelIndexSectors,(char*)thirdLevelIndex);
        }*/
        delete secondaryIndex; delete thirdLevelIndex;
        numSectors++;
        return sector;
    }else{
        DEBUG('f',"Too large.\n");
        Abort();
    }

}
//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    /*
    for (int i = 0; i < numSectors; i++) {
	ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	freeMap->Clear((int) dataSectors[i]);
    }*/

    int i = 0;
    int firstLevel = min(numSectors,NumFirstLevelDirect);
    for (; i < firstLevel; i++){
        /*
        dataSectors[i] = freeMap->Find();
        DEBUG('f',"Filehdr::Allocate: 1st level: dataSectors[i]:%d\n",dataSectors[i]);*/
        DEBUG('f',"Filehdr::Deallocate: 1st level: dataSectors[%d]\n",i);
        ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
        freeMap->Clear((int) dataSectors[i]);
    }
    if(i<numSectors){// need secondary index
        SecondaryIndex * secondaryIndex = new SecondaryIndex;
        synchDisk->ReadSector(SecondaryIndexSectors,(char*)secondaryIndex);
        int secondLevel = min(numSectors - NumFirstLevelDirect , NumIndexDirect);
        for(i = 0;i<secondLevel;++i){
            freeMap->Clear(secondaryIndex->dataSectors[i]);
            DEBUG('f',"Filehdr::Deallocate: 2st level: sec->dataSectors[%d]\n",i);
        }
        delete secondaryIndex;
        freeMap->Clear(SecondaryIndexSectors);
        if( (i+NumFirstLevelDirect) < numSectors){// need third level director
            
            ThirdLevelIndex * thirdLevelIndex = new ThirdLevelIndex;
            synchDisk->ReadSector(ThirdLevelIndexSectors,(char*)thirdLevelIndex);

            int sectors_left = numSectors - NumFirstLevelDirect - NumIndexDirect;
            ASSERT(sectors_left <= NumIndexDirect*NumIndexDirect )
            int idx_1 = sectors_left / NumIndexDirect;
            int idx_2 = sectors_left % NumIndexDirect;

            //apart from the last entry.
            for(i = 0;i<idx_1-1;++i){
                SecondaryIndex * secondaryIndex = new SecondaryIndex;
                synchDisk->ReadSector(thirdLevelIndex->SecondaryIndexSectors[i],(char*)secondaryIndex);
                for(int j = 0 ;j < NumIndexDirect;++j){
                        freeMap->Clear(secondaryIndex->dataSectors[j]);
                }
                DEBUG('f',"Filehdr::Deallocate: 3rd level: third->sec[%d]\n",i);
                delete secondaryIndex;
                freeMap->Clear(thirdLevelIndex->SecondaryIndexSectors[i]);
            }
            
            // the last entry
            SecondaryIndex * secondaryIndex = new SecondaryIndex;
            synchDisk->ReadSector(thirdLevelIndex->SecondaryIndexSectors[idx_1-1],(char*)secondaryIndex);
            for(int j = 0 ;j < idx_2;++j){
                    freeMap->Clear(secondaryIndex->dataSectors[j]);
            }
            DEBUG('f',"Filehdr::Deallocate: 3rd level: third->sec[%d]\n",i);
            delete secondaryIndex;
            freeMap->Clear(thirdLevelIndex->SecondaryIndexSectors[idx_1-1]);
            
            // third level index sectors itself.
            freeMap->Clear(ThirdLevelIndexSectors);
            delete thirdLevelIndex;
        }
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
    
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    int blockIndex = offset/SectorSize;
    ASSERT(blockIndex>=0);
    if(blockIndex < NumFirstLevelDirect){
        DEBUG('f',"FileHeader::ByteToSector: using first level indexing. SectorNum:%d\n",dataSectors[blockIndex]);
        return(dataSectors[blockIndex]);
    }else{// using higher level of indexing
        DEBUG('f',"FileHeader::ByteToSector: using higher level indexing - ");
        blockIndex = blockIndex - NumFirstLevelDirect;
        ASSERT(blockIndex>=0);
        if( blockIndex < NumIndexDirect){//using secondary indexing
            DEBUG('f'," 2-level index.\n");
            SecondaryIndex * secondaryIndex=new SecondaryIndex;
            DEBUG('f',"SecondaryIndexSectors:%d\n",SecondaryIndexSectors);
            synchDisk->ReadSector(SecondaryIndexSectors, (char *)secondaryIndex);
            DEBUG('f'," \t* IndexSectorNum:%d, DataSectorNum:%d\n",
                SecondaryIndexSectors,secondaryIndex->dataSectors[blockIndex]);
            int sector = secondaryIndex->dataSectors[blockIndex];
            delete secondaryIndex;
            return sector;
        }else{//using third level indexing
            DEBUG('f'," 3-level index.\n");
            blockIndex = blockIndex - NumIndexDirect;
            int blockIndex_1 = blockIndex / NumIndexDirect;
            int blockIndex_2 = blockIndex % NumIndexDirect;
            ASSERT(blockIndex>=0 && blockIndex_1>=0 && blockIndex_2>=0);

            ThirdLevelIndex * thirdLevelIndex= new ThirdLevelIndex;
            SecondaryIndex * secondaryIndex=new SecondaryIndex;
            synchDisk->ReadSector(ThirdLevelIndexSectors, (char *)thirdLevelIndex);
            synchDisk->ReadSector(thirdLevelIndex->SecondaryIndexSectors[blockIndex_1],
                                    (char *)secondaryIndex);

            DEBUG('f'," \t* 3-Index SectorNum:%d  2-IndexSectorNum:%d, DataSectorNum:%d\n",
                ThirdLevelIndexSectors,thirdLevelIndex->SecondaryIndexSectors[blockIndex_1],
                secondaryIndex->dataSectors[blockIndex]);
            int sector = secondaryIndex->dataSectors[blockIndex_2];
            delete secondaryIndex; delete thirdLevelIndex;
            return(sector);
        }
    }
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < numSectors; i++)
	printf("%d ", dataSectors[i]);
        if(isDirectory()) printf(" \nisDirector? YES \n");
    else printf(" \nisDirectory? NO \n");
    printf("\nFile contents:\n");
    for (i = k = 0; i < NumFirstLevelDirect; i++) {
	    synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
    delete [] data;
}

