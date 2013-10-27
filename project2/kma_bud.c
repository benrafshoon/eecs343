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

inline unsigned int GetSize(Block* block);

inline int IsAllocated(Block* block);

inline void SetAllocated(Block* block);

inline void SetFree(Block* block);

inline void SetIsAllocated(Block* block, int isAllocated);

inline Block* GetBlockFromPointer(void* pointer);

inline void* GetPointerFromBlock(Block* block);

inline Page* GetPageFromPointer(void* pointer);

inline FirstPage* GetFirstPage();

Block* SplitFreeBlock(Block* block, unsigned int size);

void InitializeFirstPage();

void AddBlockToFreeList(FreeBlock* toAdd); //Currently O(1)

void RemoveBlockFromFreeList(FreeBlock* toRemove); //Currently O(n)

unsigned int NextPowerOf2(unsigned int number);

unsigned int Log2(unsigned int number);

void PrintFreeList();

void PrintPage(Page* page);

Block* FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(unsigned int size);

void AllocatePage();

inline Block* LowerBlock(Block* block1, Block* block2);

Block* CoalesceBlock(Block* block);

void FreePage(Page* page);

void AttemptToFreeFirstPage();

/************External Declaration*****************************************/

/**************Implementation***********************************************/


inline Block* GetBuddy(Block* block) {
    //Because block->size is unsigned, when upcasting from int to size_t, size should be zero extended, not sign extended
    //This shouldn't be an issue on x86 because sizeof(int) == sizeof(void*) == sizeof(size_t) == 4 == 32 bits
    return (Block*)((size_t)block ^ GetSize(block));
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

inline FirstPage* GetFirstPage() {
    return (FirstPage*)firstPageT->ptr;
}



inline Block* LowerBlock(Block* block1, Block* block2) {
    return (size_t)block1 < (size_t)block2 ? block1 : block2;
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
    Block* currentBlock = block;
    unsigned int currentSize;
    while((currentSize = GetSize(currentBlock)) > size) {
        unsigned int newSize = currentSize >> 1; //New blocks are half the size of the old ones
        Block* leftBlock = block;
        leftBlock->size = newSize;
        Block* rightBlock = (Block*)((size_t)block + newSize);
        rightBlock->size = newSize;
        AddBlockToFreeList((FreeBlock*)rightBlock);
        currentBlock = leftBlock;
    }
    return currentBlock;
}

void AddBlockToFreeList(FreeBlock* toAdd) {
    FirstPage* firstPage = GetFirstPage();
    unsigned int listIndex = Log2(toAdd->size) - 3;
    toAdd->nextFreeBlock = firstPage->freeList[listIndex];
    firstPage->freeList[listIndex] = toAdd;
}

void RemoveBlockFromFreeList(FreeBlock* toRemove) {
    FirstPage* firstPage = GetFirstPage();
    unsigned int listIndex = Log2(toRemove->size) - 3;
    FreeBlock* block = firstPage->freeList[listIndex];
    FreeBlock* previousBlock = NULL;
    while(block != NULL && block != toRemove) {
        previousBlock = block;
        block = block->nextFreeBlock;
    }
    if(firstPage->freeList[listIndex] == toRemove) {
        firstPage->freeList[listIndex] = toRemove->nextFreeBlock;
    }
    if(previousBlock != NULL) {
        previousBlock->nextFreeBlock = toRemove->nextFreeBlock;
    }
}

void InitializeFirstPage() {
    if(firstPageT == NULL) {
        firstPageT = get_page();
        FirstPage* firstPage = GetFirstPage();
        firstPage->block.size = firstPageT->size;
        firstPage->numAllocatedBlocks = 1;
        int i;
        for(i = 0; i < FREELIST_SIZE; i++) {
            firstPage->freeList[i] = NULL;
        }
        Block* firstPageBlock = SplitFreeBlock((Block*)firstPage, 64);
        SetAllocated(firstPageBlock);
    }
}

void PrintFreeList() {
    FirstPage* firstPage = firstPageT->ptr;
    int i;
    for(i = 0; i < FREELIST_SIZE; i++) {
        unsigned int currentSize = 8 << i;
        printf("Size %i blocks:\n", currentSize);
        FreeBlock* currentBlock = firstPage->freeList[i];
        while(currentBlock != NULL) {
            printf("  %p - %i bytes\n", currentBlock, currentBlock->size);
            if(GetSize((Block*)currentBlock) != currentSize) {
                printf("SIZE MISMATCH\n");
                exit(1);
            }
            currentBlock = currentBlock->nextFreeBlock;

        }
    }
}

inline Page* GetPageFromPointer(void* pointer) {
    return (Page*)((size_t)pointer & ~0x1FFF);
}

void PrintPage(Page* page) {
    printf("Printing page %p\n", page);
    Block* currentBlock = (Block*)page;
    while((size_t)currentBlock < (size_t)page + PAGESIZE) {
        printf("%p - ", currentBlock);
        unsigned int size = GetSize(currentBlock);
        printf("%i - ", size);
        if(IsAllocated(currentBlock)) {
            printf("ALLOCATED");
        } else {
            printf("FREE");
        }
        printf("\n");
        currentBlock = (Block*)((size_t)currentBlock + size);
    }
}

Block* FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(unsigned int size) {
    unsigned int sizeIndex = Log2(size) - 3;
    FirstPage* firstPage = GetFirstPage();

    FreeBlock* freeBlock = NULL;
    int i;
    for(i = sizeIndex; i < FREELIST_SIZE; i++) {
        freeBlock = firstPage->freeList[i];
        if(freeBlock != NULL) {
            firstPage->freeList[i] = freeBlock->nextFreeBlock;
            break;
        }
    }
    return (Block*)freeBlock;
}

void AllocatePage() {
    kma_page_t* page_t = get_page();
    Page* page = (Page*)page_t->ptr;
    page->page_t = page_t;
    page->numAllocatedBlocks = 0;
    page->block.size = page_t->size;
    Block* pageBlock = SplitFreeBlock((Block*)page, 16);
    SetAllocated(pageBlock);
    PrintPage((Page*)page);
    PrintFreeList();
}

Block* CoalesceBlock(Block* block) {
    Block* buddy = GetBuddy(block);
    while(GetSize(block) < PAGESIZE && !IsAllocated(buddy) && GetSize(block) == GetSize(buddy)) {
        printf("Coalescing block %p of size %i\n", block, GetSize(block));
        printf("Buddy is %p of size %i - ", buddy, GetSize(buddy));
        if(IsAllocated(buddy)) {
            printf("ALLOCATED");
        } else {
            printf("FREE");
        }
        printf("\n");
        RemoveBlockFromFreeList((FreeBlock*)buddy);
        Block* leftBlock = LowerBlock(block, buddy);
        leftBlock->size = GetSize(block) * 2; //implicitly frees block
        block = leftBlock;
        buddy = GetBuddy(block);
    }
    return block;
}

void FreePage(Page* page) {
    printf("Freeing page %p\n", page);
    Block* pageBlock = (Block*)page;
    CoalesceBlock(pageBlock);
    PrintPage(page);
    free_page(page->page_t);
}

void AttemptToFreeFirstPage() {
    FirstPage* firstPage = GetFirstPage();
    kma_page_stat_t* currentPageStats = page_stats();
    if(firstPage->numAllocatedBlocks == 1 && currentPageStats->num_in_use == 1) {
        printf("Freeing first page\n");
        free_page(firstPageT);
        firstPageT = NULL;
    }
}

void* kma_malloc(kma_size_t size) {
    InitializeFirstPage();
    unsigned int blockSize = NextPowerOf2(size + sizeof(Block));
    if(blockSize == PAGESIZE) {
        printf("8K blocks not yet implemented\n");
        return NULL;
    }
    printf("Requested block of size %i, allocating block of size %i\n", size, blockSize);

    Block* toAllocate = FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(blockSize);
    if(toAllocate == NULL) {
        printf("Could not find free block.  Need to request another page\n");
        AllocatePage();
        toAllocate = FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(blockSize);
    }

    printf("Found block %p of size %i\n", toAllocate, GetSize(toAllocate));
    toAllocate = SplitFreeBlock(toAllocate, blockSize);
    SetAllocated(toAllocate);
    Page* pageOfBlockToAllocate = GetPageFromPointer((void*)toAllocate);
    pageOfBlockToAllocate->numAllocatedBlocks++;
    printf("Incrementing number of allocated blocks in page %p, now %i allocated blocks\n",
           pageOfBlockToAllocate,
           pageOfBlockToAllocate->numAllocatedBlocks
           );
    printf("Allocating block %p of size %i\n", toAllocate, GetSize(toAllocate));
    PrintPage(pageOfBlockToAllocate);
    PrintFreeList();
    printf("\n\n\n");
    //getchar();
    return GetPointerFromBlock(toAllocate);
}



void kma_free(void* ptr, kma_size_t size) {
    Block* block = GetBlockFromPointer(ptr);
    Page* page = GetPageFromPointer((void*)block);

    printf("Freeing block %p from page %p\n", block, page);
    PrintPage(page);
    SetFree(block);
    block = CoalesceBlock(block);
    printf("Adding block %p of size %i to free list\n", block, GetSize(block));
    AddBlockToFreeList((FreeBlock*)block);
    page->numAllocatedBlocks--;
    printf("Page %p has %i allocated blocks\n", page, page->numAllocatedBlocks);
    if(page->numAllocatedBlocks == 0) {
        FreePage(page);
    }
    PrintFreeList();
    AttemptToFreeFirstPage();

    //getchar();
    printf("\n\n\n");
}


#endif // KMA_BUD
