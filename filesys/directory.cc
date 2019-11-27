// directory.cc 
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//	Fixing this is one of the parts to the assignment.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory(int size)
{
    table = new DirectoryEntry[size];
    entryCnt = tableSize = size;
    for (int i = 0; i < tableSize; i++)
    table[i].FLAG ^= IN_USE;
	//table[i].inUse = FALSE;
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{ 
    delete [] table;
} 

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void
Directory::FetchFrom(OpenFile *file)
{
    (void) file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void
Directory::WriteBack(OpenFile *file)
{
    (void) file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::FindIndex(char *name)
{
    for (int i = 0; i < tableSize; i++)
        if(table[i].FLAG & LONG_NAME){
            DEBUG('f',"LONG NAME:");
            char filename[FileNameMaxLen+ExtendedFileNameMaxLen+1];
            strncpy(filename,table[i].name, FileNameMaxLen+1); // use the space for '/0' 
            strncpy(filename + FileNameMaxLen+1, ((LongNameDirectoryEntry*) (table+i+1))->name,ExtendedFileNameMaxLen); 
            if ((table[i].FLAG & IN_USE) && !strncmp(filename, name, FileNameMaxLen+ExtendedFileNameMaxLen)){
                return i;
            }
            i++;
        }else{
            if ((table[i].FLAG & IN_USE) && !strncmp(table[i].name, name, FileNameMaxLen))
            return i;
        }
    return -1;		// name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't 
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::Find(char *name)
{
    int i = FindIndex(name);

    if (i != -1)
	return table[i].sector;
    return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------

bool
Directory::Add(char *name, int newSector,bool hasLongName)
{ 
    if(!hasLongName){
        if (FindIndex(name) != -1)
        return FALSE;

        bool found = false;
        for (int i = 0; i < tableSize; i++)
            if ((table[i].FLAG & IN_USE)==0) {// Not in used
                DEBUG('f',"Direct::Add: Found %d-th entry. Allocate for %s.\n",i,name);
                table[i].FLAG ^= IN_USE;
                strncpy(table[i].name, name, FileNameMaxLen); 
                table[i].sector = newSector;
                found = true;
                return TRUE;
            }
        if(!found){
            DEBUG('f',"Append new entry of %s.\n",name);
            DirectoryEntry * new_table = new DirectoryEntry[ (tableSize+1) * sizeof(DirectoryEntry)];
            bcopy(table,new_table,(tableSize) * sizeof(DirectoryEntry));
            delete [] table;
            table = new_table;
            tableSize++; entryCnt++;
            table[tableSize-1].FLAG = IN_USE;
            DEBUG('f',"Direct::Add: New %d-th entry. Allocate for %s.\n",tableSize-1,name);
            strncpy(table[tableSize-1].name, name, FileNameMaxLen); 
            table[tableSize-1].sector = newSector;
            return TRUE;
        }
        return FALSE;	// no space.  Fix when we have extensible files. -- now we have extensible files
    }else{//has long name
        bool found = false;
        for (int i = 0; i < tableSize; i++)
            if ((table[i].FLAG & IN_USE)==0 && (table[i+1].FLAG & IN_USE)==0 ) {// Not in used
                DEBUG('f',"Direct::Add: Found %d-th and %d-th entry. Allocate for %s.(LONG NAME)\n",i,i+1,name);
                table[i+1].FLAG = table[i].FLAG = table[i].FLAG ^ IN_USE;
                strncpy(table[i].name, name, FileNameMaxLen+1); // use the space for '/0' 
                strncpy( ((LongNameDirectoryEntry*) (table+i+1))->name, name + FileNameMaxLen + 1, ExtendedFileNameMaxLen); 
                table[i].sector = newSector;
                found = true;
                return TRUE;
            }
        if(!found){
            DEBUG('f',"Append new entry of %s.\n",name);
            DirectoryEntry * new_table = new DirectoryEntry[ (tableSize+1) * sizeof(DirectoryEntry)];
            bcopy(table,new_table,(tableSize) * sizeof(DirectoryEntry));
            delete [] table;
            table = new_table;
            tableSize+=2; entryCnt++;
            table[tableSize-2].FLAG = (IN_USE | LONG_NAME);
            table[tableSize-1].FLAG = (IN_USE);
            DEBUG('f',"Direct::Add: New %d-th and %d-th entry. Allocate for %s.\n",tableSize-2,tableSize-1,name);
            strncpy(table[tableSize-2].name, name, FileNameMaxLen+1); // use the space for '/0' 
            strncpy( ((LongNameDirectoryEntry*) (table+tableSize-1))->name, name + FileNameMaxLen + 1, ExtendedFileNameMaxLen); 
            table[tableSize-1].sector = newSector;
            return TRUE;
        }
    }
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory. 
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool
Directory::Remove(char *name)
{ 
    int i = FindIndex(name);

    if (i == -1)
	return FALSE; 		// name not in directory
    table[i].FLAG &= (~IN_USE);
    if(table[i].FLAG & LONG_NAME){
        table[i+1].FLAG &= (~IN_USE);
    }
    return TRUE;	
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory. 
//----------------------------------------------------------------------

void
Directory::List()
{
   for (int i = 0; i < tableSize; i++)
	if (table[i].FLAG & IN_USE){
        if(table[i].FLAG & LONG_NAME){
            DEBUG('f',"LONG NAME:");
            char filename[FileNameMaxLen+ExtendedFileNameMaxLen+1];
            strncpy(filename,table[i].name, FileNameMaxLen+1); // use the space for '/0' 
            strncpy(filename + FileNameMaxLen+1, ((LongNameDirectoryEntry*) (table+i+1))->name,ExtendedFileNameMaxLen); 
	        printf("%s\n", filename);
            i++;
        }
        else
	        printf("%s\n", table[i].name);
    }
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

void
Directory::Print()
{ 
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (int i = 0; i < tableSize; i++)
	if (table[i].FLAG & IN_USE) {
	    printf("Name: %s, Sector: %d\n", table[i].name, table[i].sector);
	    hdr->FetchFrom(table[i].sector);
	    hdr->Print();
	}
    printf("\n");
    delete hdr;
}
