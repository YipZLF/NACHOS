// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte) //32*32/8 = 128
#define NumDirEntries 		10
#define DirectoryFileSize 	(sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{ 
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
	FileHeader *mapHdr = new FileHeader;
	FileHeader *dirHdr = new FileHeader(0,DIRECTORY_FILE);

        DEBUG('f', "Formatting the file system.\n");

    // First, allocate space for FileHeaders for the directory and bitmap
    // (make sure no one else grabs these!)
	freeMap->Mark(FreeMapSector);	    
	freeMap->Mark(DirectorySector);

    // Second, allocate space for the data blocks containing the contents
    // of the directory and bitmap files.  There better be enough space!

	ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
	ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

    // Flush the bitmap and directory FileHeaders back to disk
    // We need to do this before we can "Open" the file, since open
    // reads the file header off of disk (and currently the disk has garbage
    // on it!).

        DEBUG('f', "Writing headers back to disk.\n");
	mapHdr->WriteBack(FreeMapSector);    
	dirHdr->WriteBack(DirectorySector);

    // OK to open the bitmap and directory files now
    // The file system operations assume these two files are left open
    // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
     
    // Once we have the files "open", we can write the initial version
    // of each file back to disk.  The directory at this point is completely
    // empty; but the bitmap has been changed to reflect the fact that
    // sectors on the disk have been allocated for the file headers and
    // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);	 // flush changes to disk
        
        directory->InitialEntry(DirectorySector,-1);
        directory->WriteBack(directoryFile);

	if (DebugIsEnabled('f')) {
	    freeMap->Print();
	    directory->Print();

        delete freeMap; 
	delete directory; 
	delete mapHdr; 
	delete dirHdr;
	}
    } else {
    // if we are not formatting the disk, just open the files representing
    // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

OpenFile* FileSystem::findFatherDirbyPath(char * name){
    /* every path should start with '/' indicating it searches from the root directory 
    if not ,the default behaviour is to start searching from root */

    int maxlen = 0,curpos = ( (name[0]=='/')?1:0 );
    for(; name[maxlen];++maxlen);//get name length
    
    DEBUG('f',"Start trying to resolve path: %s length:%d\n",name,maxlen);

    char cur_name[FileNameMaxLen];
    Directory * cur_directory;    
    OpenFile* openFile = directoryFile;
  

    cur_directory = new Directory(NumDirEntries);
    cur_directory->FetchFrom(directoryFile);    
    
    while(curpos<=maxlen){
        int i = curpos,name_length;
        for(;i < maxlen; ++i){
            if(name[i] == '/')
                break;
        }

        name_length = i-curpos;
        bzero(cur_name,sizeof(cur_name));
        bcopy(&name[curpos],cur_name,name_length);
        
        DEBUG('f',"\t * cur_name:%s, len:%d i:%d\t",
                cur_name, name_length,i);
        
        if(i==maxlen){
            bcopy(cur_name,name,name_length+1);

            DEBUG('f',"Found %s! \n",name);
            delete cur_directory;
            if(DebugIsEnabled('f')){
                openFile->getHeader()->Print();
            }
            return openFile;    
        }else{
            int sector  = cur_directory->Find(cur_name);
            DEBUG('f',"Keep on looking at sector: %d\n",sector);
            if(sector<0){
                DEBUG('f',"Failed to find sector of %s\n",cur_name);
                delete cur_directory;
                return NULL;
            }
            if(openFile!=directoryFile) delete openFile;
            openFile = new OpenFile(sector);
            ASSERT(openFile->getHeader()->isDirectory());
            curpos = i+1;
            cur_directory->FetchFrom(openFile);
        }
    }
}

bool
FileSystem::Create(char *name, int initialSize, bool makeDir)
{
    Directory *directory;
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;
    OpenFile  *fatherDirFile = NULL;

    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);

    fatherDirFile = findFatherDirbyPath(name);
    if(DebugIsEnabled('f')){
        fatherDirFile->getHeader()->Print();
    }
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(fatherDirFile);
    int old_table_size = directory->GetTableSize();
    if (directory->Find(name) != -1){
      DEBUG('f',"Create:: file is already in directory\n");
      success = FALSE;			// file is already in directory
    }else{	
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        if(DebugIsEnabled('f')){
            DEBUG('f',"FREE MAP in CREATE:\n");
            freeMap->Print();
        }
        sector = freeMap->Find();	// find a sector to hold the file header
    	if (sector == -1) 		
            success = FALSE;		// no free block for file header 
        //else if (!directory->Add(name, sector))
        //    success = FALSE;	// no space in directory
        else {
            directory->Add(name, sector);
            int fileSize = 0;
            
            if(makeDir){
                DEBUG('f',"Create:: Create directory file %s\n",name);
                hdr = new FileHeader(0,DIRECTORY_FILE);
                fileSize = 2*sizeof(int) + NumDirEntries * sizeof(DirectoryEntry);// hardcoded to NumDirEntries entries
                ASSERT(hdr->Allocate(freeMap, fileSize));
                
            }else{  // user file
                DEBUG('f',"Create:: Create user file %s\n",name);
                hdr = new FileHeader();
                fileSize = initialSize;
                if(fileSize == 0) fileSize++;
                ASSERT(hdr->Allocate(freeMap, fileSize));
            }
            
	
            DEBUG('f',"Create:: everthing worked, flush all changes back to disk\n");
            success = TRUE;
            // everthing worked, flush all changes back to disk
            hdr->WriteBack(sector); 
            
            if(directory->GetTableSize()!=old_table_size){
                DEBUG('f',"Create:: add new entry to directory and it's been larger. Need 1 more sector\n");
            }
            directory->WriteBack(fatherDirFile);
            freeMap->WriteBack(freeMapFile);
            if(makeDir){
                Directory * new_dir = new Directory(NumDirEntries);
                OpenFile * new_dir_openfile = new OpenFile(sector);
                
                new_dir->InitialEntry(sector,directory->Find("."));

                new_dir->WriteBack(new_dir_openfile);                
                if(DebugIsEnabled('f')){
                    new_dir->Print();
                }
                delete new_dir; delete new_dir_openfile;

            }
            delete hdr;
	    }
        delete freeMap;
    }
    delete directory;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//	To open a file:
//	  Find the location of the file's header, using the directory 
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(char *name)
{ 
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL,*fatherDirFile = NULL;
    int sector;
    
    fatherDirFile = findFatherDirbyPath(name);
    DEBUG('f', "Opening file %s\n", name);
    directory->FetchFrom(fatherDirFile);
    sector = directory->Find(name); 
    DEBUG('f',"\t **Opening file %s at sector %d\n",name,sector);
    if (sector >= 0) 		
	openFile = new OpenFile(sector);	// name was found in directory 
    delete directory;
    return openFile;				// return NULL if not found
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool
FileSystem::Remove(char *name)
{ 
    Directory *directory;
    BitMap *freeMap;
    FileHeader *fileHdr;
    OpenFile  *fatherDirFile = NULL;
    int sector;
    
    fatherDirFile = findFatherDirbyPath(name);

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(fatherDirFile);
    sector = directory->Find(name);
    if (sector == -1) {
       delete directory;
       return FALSE;			 // file not found 
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap);  		// remove data blocks
    freeMap->Clear(sector);			// remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);		// flush to disk
    directory->WriteBack(fatherDirFile);        // flush to disk
    delete fileHdr;
    delete directory;
    delete freeMap;
    return TRUE;
} 

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::List(char* name)
{
    OpenFile  *fatherDirFile = NULL;
    
    Directory *directory = new Directory(NumDirEntries);
    if(name ==NULL){
        fatherDirFile = directoryFile;
        directory->FetchFrom(fatherDirFile);
    }else{
        fatherDirFile = findFatherDirbyPath(name);
        directory->FetchFrom(fatherDirFile);
        int sector = directory->Find(name);
        OpenFile name_dir(sector);
        directory->FetchFrom(&name_dir);
    }

    directory->List();
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
} 

int FileSystem::AllocateOneMoreSector(OpenFile * file){
    DEBUG('f',"FileSystem: Trying to allocate one more sector\n");
    FileHeader* hdr =  file->getHeader();
    BitMap * freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);
    int sector = hdr->AppendOneSector(freeMap);
    freeMap->WriteBack(freeMapFile);
    return sector;
}

