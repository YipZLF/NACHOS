// filehdr.h 
//	Data structures for managing a disk file header.  
//
//	A file header describes where on disk to find the data in a file,
//	along with other information about the file (for instance, its
//	length, owner, etc.)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "bitmap.h"

#define NumDirect 	((SectorSize - 6 * sizeof(int)) / sizeof(int))
#define MaxFileSize 	(NumDirect * SectorSize)

// The following class defines the Nachos "file header" (in UNIX terms,  
// the "i-node"), describing where on disk to find all of the data in the file.
// The file header is organized as a simple table of pointers to
// data blocks. 
//
// The file header data structure can be stored in memory or on disk.
// When it is on disk, it is stored in a single sector -- this means
// that we assume the size of this data structure to be the same
// as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// There is no constructor; rather the file header can be initialized
// by allocating blocks for the file (if it is a new file), or by
// reading it from disk.

// Mixed indexing of data.
#define NumIndexDirect (SectorSize / sizeof(int))
#define NumFirstLevelDirect (NumDirect-2)
struct SecondaryIndex{
  int dataSectors[NumIndexDirect];
};
struct ThirdLevelIndex{
  int SecondaryIndexSectors[NumIndexDirect];
};


#define DIRECTORY_FILE 1
#define USER_FILE 0

class FileHeader {
  public:
    FileHeader(int cur_time = 0,unsigned int _flag = USER_FILE);
    bool Allocate(BitMap *bitMap, int fileSize);// Initialize a file header, 
						//  including allocating space 
						//  on disk for the file data
    int AppendOneSector(BitMap *bitmap);
    void Deallocate(BitMap *bitMap);  		// De-allocate this file's 
						//  data blocks

    void FetchFrom(int sectorNumber); 	// Initialize file header from disk
    void WriteBack(int sectorNumber); 	// Write modifications to file header
					//  back to disk

    int ByteToSector(int offset);	// Convert a byte offset into the file
					// to the disk sector containing
					// the byte

    int FileLength();			// Return the length of the file 
					// in bytes

    void Print();			// Print the contents of the file.
    int SetFileLength(int new_file_length){numBytes = new_file_length;}
  private:
    int numBytes;			// Number of bytes in the file
    int numSectors;			// Number of data sectors in the file
    unsigned int flag;
    int create_time;
    int last_modified_time;
    int last_access_time;
    int dataSectors[NumFirstLevelDirect];		// Disk sector numbers for each data 
					// block in the file
    int SecondaryIndexSectors;
    int ThirdLevelIndexSectors;
    /*
    if numSectors is within:
      (0 , NumFirstLevelDirect) : NumNewBlocks = numSectors
      (NumFirstLevelDirect+1 , NumFirstLevelDirect + NumIndexDirect): numSectors+1
      (NumFirstLevelDirect + NumIndexDirect+1, NumFirstLevelDirect + NumIndexDirect + NumIndexDirect^2):numSectors + 1  + 1 + # of Secondary Index

    */
};



#endif // FILEHDR_H
