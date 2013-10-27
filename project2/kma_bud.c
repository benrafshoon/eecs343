/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 ***************************************************************************/
#ifdef KMA_BUD
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

 #define MIN_BLOCK_SIZE 8

//Max block size = 8k == 2^13
//Need to store size and is allocated
typedef struct Block_ {
    unsigned int size;
} Block;

typedef struct FreeBlock_ {
    unsigned int size;
    struct FreeBlock_* nextFreeBlock;
} FreeBlock;

//All pages have a block at the beginning containing metadata.  This does not count as an allocated block
//except in the first page
typedef struct Page_ {
    Block block;
    kma_page_t* page_t;
    unsigned int numAllocatedBlocks;
} Page;

//FREELIST_SIZE = log2(4096) - log2(8) + 1
#define FREELIST_SIZE 10

//In the first page, the first block counts as an allocated block since it contains the free list
//This ensures that the free list is never deleted
typedef struct FirstPage_ {
    Block block;
    kma_page_t* page_t;
    unsigned int numAllocatedBlocks;
    FreeBlock* freeList[FREELIST_SIZE];
    //freeList[0] contains 8B blocks
    //freeList[1] contains 16B blocks
    //...
    //freeList[9] contains 4KB blocks
    //8KB blocks are special cases because a new page is always allocated for 8KB blocks
} FirstPage;

/************Global Variables*********************************************/

static kma_page_t* firstPageT = NULL;

/************Function Prototypes******************************************/

inline Block* GetBuddy(Block* block);

inline int IsAllocated(Block* block);

inline void SetAllocated(Block* block);

inline void SetFree(Block* block);

inline void SetIsAllocated(Block* block, int isAllocated);

inline Block* GetBlockFromPointer(void* pointer);

inline void* GetPointerFromBlock(Block* block);

Block* SplitFreeBlock(Block* block, unsigned int size);

void InitializeFirstPage();

void AddBlockToFreeList(FreeBlock* block);

unsigned int NextPowerOf2(unsigned int number);

unsigned int Log2(unsigned int number);

void PrintFreeList();

/************External Declaration*****************************************/

/**************Implementation***********************************************/


inline Block* GetBuddy(Block* block) {
    //Because block->size is unsigned, when upcasting from int to size_t, size should be zero extended, not sign extended
    //This shouldn't be an issue on x86 because sizeof(int) == sizeof(void*) == sizeof(size_t) == 4 == 32 bits
    return (Block*)((size_t)block ^ block->size);
}

inline unsigned int GetSize(Block* block) {
    return block->size & (~ 0x1);
}

inline int IsAllocated(Block* block) {
    return block->size & 0x1;
}

inline void SetAllocated(Block* block) {
    block->size = block->size | 0x1;
}

inline void SetFree(Block* block) {
    block->size = block->size & (~ 0x1);
}

inline void SetIsAllocated(Block* block, int isAllocated) {
    if(isAllocated) {
        SetAllocated(block);
    } else {
        SetFree(block);
    }
}

inline Block* GetBlockFromPointer(void* pointer) {
    return (Block*)((size_t)pointer - sizeof(Block));
}

inline void* GetPointerFromBlock(Block* block) {
    return (void*)((size_t)block + sizeof(Block));
}

unsigned int NextPowerOf2(unsigned int number) {
    number--;
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
    number++;
    return number;

}

unsigned int Log2(unsigned int number) {
    unsigned int log2 = 0;
    while(number >>= 1) {
        log2++;
    }
    return log2;
}

//Size is the size of the entire block in bytes.  It does not take into account space to store block metadata
//(ie size, allocation status)
Block* SplitFreeBlock(Block* block, unsigned int size) {
    printf("SplitFreeBlock block %p size %i\n", block, size);
    Block* currentBlock = block;
    unsigned int currentSize;
    while((currentSize = GetSize(currentBlock)) > size) {
        printf("Splitting block %p of size %i\n", currentBlock, currentSize);
        unsigned int newSize = currentSize >> 1; //New blocks are half the size of the old ones
        Block* leftBlock = block;
        leftBlock->size = newSize;
        printf("Left block %p of size %i\n", leftBlock, leftBlock->size);
        Block* rightBlock = (Block*)((size_t)block + newSize);
        rightBlock->size = newSize;
        printf("Right block %p of size %i\n", rightBlock, rightBlock->size);
        AddBlockToFreeList((FreeBlock*)rightBlock);
        currentBlock = leftBlock;
    }
    printf("Returning block %p of size %i\n", currentBlock, currentBlock->size);
    return currentBlock;
}

void AddBlockToFreeList(FreeBlock* toAdd) {
    FirstPage* firstPage = firstPageT->ptr;
    unsigned int listIndex = Log2(toAdd->size) - 3;
    printf("FreeBlock list index %i\n", listIndex);
    FreeBlock* head = firstPage->freeList[listIndex];

    FreeBlock* currentBlock = head;
    while(currentBlock != NULL) {
        currentBlock = currentBlock->nextFreeBlock;
    }
    if(currentBlock == NULL) {
        firstPage->freeList[listIndex] = toAdd;
    } else {
        currentBlock->nextFreeBlock = toAdd;
    }
    toAdd->nextFreeBlock = NULL;
}

void InitializeFirstPage() {
    if(firstPageT == NULL) {
        firstPageT = get_page();

        FirstPage* firstPage = (FirstPage*)firstPageT->ptr;
        firstPage->block.size = firstPageT->size;
        firstPage->numAllocatedBlocks = 1;
        int i;
        for(i = 0; i < FREELIST_SIZE; i++) {
            firstPage->freeList[i] = NULL;
        }
        SplitFreeBlock((Block*)firstPage, 64);
    }
    PrintFreeList();

}

void PrintFreeList() {
    FirstPage* firstPage = firstPageT->ptr;
    int i;
    for(i = 0; i < FREELIST_SIZE; i++) {
        int currentSize = 8 << i;
        printf("Size %i blocks:\n", currentSize);
        FreeBlock* currentBlock = firstPage->freeList[i];
        while(currentBlock != NULL) {
            printf("  %p - %i bytes\n", currentBlock, currentBlock->size);
            currentBlock = currentBlock->nextFreeBlock;
        }
    }
}

void* kma_malloc(kma_size_t size) {
    InitializeFirstPage();
    /*kma_page_t* page_t = get_page();
    Block* baseBlock = (Block*)page_t->ptr;
    baseBlock->size = page_t->size;
    printf("Block size: %i\n", page_t->size);
    SplitFreeBlock(baseBlock, 8);*/

    return NULL;
}

void kma_free(void* ptr, kma_size_t size) {

}


#endif // KMA_BUD
