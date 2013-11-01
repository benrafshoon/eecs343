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

 #define LARGE_BLOCK_PAGE_SIZE 10

//Max block size = 8k == 2^13


typedef struct Block_ {
    struct Block_* nextBlock;
} Block;

//All pages have a block at the beginning containing metadata.  This does not count as an allocated block
//except in the first page
typedef struct Page_ {
    kma_page_t* page_t;
    int numAllocatedBlocks;
} Page;

//A LargeBlockPage is a block that can take up the entire page
typedef struct LargeBlockPage_ {
    kma_page_t* page_t;
} LargeBlockPage;

//FREELIST_SIZE = log2(4096) - log2(8) + 1
#define FREELIST_SIZE 10

//In the first page, the first block counts as an allocated block since it contains the free list
//This ensures that the free list is never deleted
typedef struct FirstPage_ {
    kma_page_t* page_t;
    int numAllocatedBlocks;
    Block* freeList[FREELIST_SIZE];
    //freeList[0] contains 8B blocks
    //freeList[1] contains 16B blocks
    //...
    //freeList[9] contains 4KB blocks
    //8KB blocks are special cases because a new page is always allocated for 8KB blocks
} FirstPage;

/************Global Variables*********************************************/

static kma_page_t* firstPageT = NULL;

/************Function Prototypes******************************************/

inline Block* GetBuddy(Block* block, int blockSize);

inline Page* GetPageFromPointer(void* pointer);

inline void* GetPointerFromLargeBlockPage(LargeBlockPage* largeBlockPage);

inline LargeBlockPage* GetLargeBlockPageFromPointer(void* pointer);

inline FirstPage* GetFirstPage();

Block* SplitFreeBlock(Block* block, int requestedSize, int currentSize);

void InitializeFirstPage();

void AddBlockToFreeList(Block* toAdd, int size); //O(1)

Block* RemoveBlockFromFreeList(Block* toRemove, int size); //O(n)

Block* FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(int requestedSize, int* actualSize); //O(1)

int IsAllocated(void* block);

inline unsigned int NextPowerOf2(unsigned int number);

inline unsigned int Log2(unsigned int number);

void PrintFreeList();

void AllocatePage();

inline Block* LowerBlock(Block* block1, Block* block2);

Block* CoalesceBlock(Block* block, int initialSize, int* coalescedSize);

void FreePage(Page* page);

void AttemptToFreeFirstPage();

LargeBlockPage* AllocateLargeBlockPage();

void FreeLargeBlockPage(LargeBlockPage* block);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

inline Block* GetBuddy(Block* block, int blockSize) {
    return (Block*)((size_t)block ^ (8 << blockSize));
}

inline FirstPage* GetFirstPage() {
    return (FirstPage*)firstPageT->ptr;
}

inline Block* LowerBlock(Block* block1, Block* block2) {
    return (size_t)block1 < (size_t)block2 ? block1 : block2;
}

inline void* GetPointerFromLargeBlockPage(LargeBlockPage* largeBlockPage) {
    return (void*)((size_t)largeBlockPage + sizeof(LargeBlockPage));
}

inline LargeBlockPage* GetLargeBlockPageFromPointer(void* pointer) {
    return (LargeBlockPage*)((size_t)pointer - sizeof(LargeBlockPage));
}

inline Page* GetPageFromPointer(void* pointer) {
    return (Page*)((size_t)pointer & ~0x1FFF);
}

inline unsigned int NextPowerOf2(unsigned int number) {
    number--;
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
    number++;
    return number;
}

inline unsigned int Log2(unsigned int number) {
    unsigned int log2 = 0;
    while(number >>= 1) {
        log2++;
    }
    return log2;
}

//Size is the size of the entire block in bytes.  It does not take into account space to store block metadata
//(ie size, allocation status)
Block* SplitFreeBlock(Block* block, int requestedSize, int currentSize) {
    //printf("Splitting block %p of size %i into size %i\n", block, currentSize, requestedSize);
    Block* currentBlock = block;
    while(currentSize > requestedSize) {
        int newSize = currentSize - 1; //New blocks are half the size of the old ones
        Block* leftBlock = block;
        Block* rightBlock = (Block*)((size_t)block + (8 << newSize));
        AddBlockToFreeList((Block*)rightBlock, newSize);
        currentBlock = leftBlock;
        currentSize = newSize;
    }
    return currentBlock;
}

void AddBlockToFreeList(Block* toAdd, int size) {
    FirstPage* firstPage = GetFirstPage();
    toAdd->nextBlock = firstPage->freeList[size];
    firstPage->freeList[size] = toAdd;
}

Block* RemoveBlockFromFreeList(Block* toRemove, int size) {
    FirstPage* firstPage = GetFirstPage();
    Block* block = firstPage->freeList[size];
    Block* previousBlock = NULL;
    while(block != NULL && block != toRemove) {
        previousBlock = block;
        block = block->nextBlock;
    }
    if(block == NULL) {
        return NULL;
    }
    if(firstPage->freeList[size] == toRemove) {
        firstPage->freeList[size] = toRemove->nextBlock;
    }
    if(previousBlock != NULL) {
        previousBlock->nextBlock = toRemove->nextBlock;
    }
    return toRemove;
}

void InitializeFirstPage() {
    if(firstPageT == NULL) {
        firstPageT = get_page();
        FirstPage* firstPage = GetFirstPage();
        firstPage->numAllocatedBlocks = 0;
        int i;
        for(i = 0; i < FREELIST_SIZE; i++) {
            firstPage->freeList[i] = NULL;
        }
        SplitFreeBlock((Block*)firstPage, 3, LARGE_BLOCK_PAGE_SIZE);
        //PrintFreeList();
    }
}

void PrintFreeList() {
    FirstPage* firstPage = firstPageT->ptr;
    int i;
    for(i = 0; i < FREELIST_SIZE; i++) {
        int currentSize = 8 << i;
        printf("Size %i blocks:\n", currentSize);
        Block* currentBlock = firstPage->freeList[i];
        while(currentBlock != NULL) {
            printf("  %p\n", currentBlock);
            currentBlock = currentBlock->nextBlock;

        }
    }
}


Block* FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(int requestedSize, int* actualSize) {
    //printf("Finding block of size %i\n", requestedSize);
    FirstPage* firstPage = GetFirstPage();
    Block* block = NULL;
    int size;
    for(size = requestedSize; size < FREELIST_SIZE; size++) {
        //printf("Size %i\n", 8 << i);
        block = firstPage->freeList[size];
        //printf("Block %p\n", block);
        if(block != NULL) {
            firstPage->freeList[size] = block->nextBlock;
            break;
        }
    }

    *actualSize = size;
    //if(block != NULL) {
       // printf("Found block %p of size %i\n", block, *actualSize);
    //}
    return block;
}

void AllocatePage() {
    kma_page_t* page_t = get_page();
    Page* page = (Page*)page_t->ptr;
    page->page_t = page_t;
    page->numAllocatedBlocks = 0;
    SplitFreeBlock((Block*)page, 0, LARGE_BLOCK_PAGE_SIZE);
    //PrintPage((Page*)page);
    //PrintFreeList();
}

Block* CoalesceBlock(Block* block, int initialSize, int* coalescedSize) {
    int size = initialSize;
    Block* buddy = GetBuddy(block, size);
    //printf("Buddy %p\n", buddy);
    while(size < LARGE_BLOCK_PAGE_SIZE && RemoveBlockFromFreeList(buddy, size) != NULL) {
        //printf("Coalescing size %i to size %i\n", size, size * 2);
        Block* leftBlock = LowerBlock(block, buddy);
        size++;
        block = leftBlock;
        buddy = GetBuddy(block, size);
    }
    *coalescedSize = size;
    return block;
}

void FreePage(Page* page) {
    //printf("Freeing page %p\n", page);
    Block* pageBlock = (Block*)page;
    int coalescedSize;
    CoalesceBlock(pageBlock, 0, &coalescedSize);
    //PrintPage(page);
    free_page(page->page_t);
}

void AttemptToFreeFirstPage() {
    FirstPage* firstPage = GetFirstPage();
    kma_page_stat_t* currentPageStats = page_stats();
    if(firstPage->numAllocatedBlocks == 0 && currentPageStats->num_in_use == 1) {
        //printf("Freeing first page\n");
        free_page(firstPageT);
        firstPageT = NULL;
    }
}

LargeBlockPage* AllocateLargeBlock() {
    kma_page_t* page_t = get_page();
    LargeBlockPage* largeBlockPage = (LargeBlockPage*)page_t->ptr;
    largeBlockPage->page_t = page_t;
    //printf("Allocating large block %p\n", largeBlock);
    return largeBlockPage;
}

void FreeLargeBlockPage(LargeBlockPage* block) {
    //printf("Freeing large block %p\n", block);
    free_page(block->page_t);
}

void* kma_malloc(kma_size_t size) {
    InitializeFirstPage();
    int blockSize = Log2(NextPowerOf2(size)) - 3;
    //printf("Requested block of size %i, allocating block of size %i\n", size, blockSize);
    if(blockSize == LARGE_BLOCK_PAGE_SIZE) {
        LargeBlockPage* largeBlockPage = AllocateLargeBlock();
        return GetPointerFromLargeBlockPage(largeBlockPage);
    }
    //PrintFreeList();
    int actualSize;
    Block* toAllocate = FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(blockSize, &actualSize);
    if(toAllocate == NULL) {
        //printf("Could not find free block.  Need to request another page\n");
        AllocatePage();
        toAllocate = FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(blockSize, &actualSize);
    }
    toAllocate = SplitFreeBlock(toAllocate, blockSize, actualSize);
    Page* pageOfBlockToAllocate = GetPageFromPointer((void*)toAllocate);
    pageOfBlockToAllocate->numAllocatedBlocks++;
    /*printf("Incrementing number of allocated blocks in page %p, now %i allocated blocks\n",
           pageOfBlockToAllocate,
           pageOfBlockToAllocate->numAllocatedBlocks
           );*/
    //printf("Allocating block %p of size %i\n", toAllocate, GetSize(toAllocate));
    //PrintPage(pageOfBlockToAllocate);
    //PrintFreeList();

    //getchar();
    //PrintFreeList();
    //printf("\n\n\n");
    return toAllocate;
}



void kma_free(void* ptr, kma_size_t size) {

    Block* block = (Block*)ptr;
    int initialSize = Log2(NextPowerOf2(size)) - 3;
    //printf("Freeing block %p of size %i\n", ptr, initialSize);
    //getchar();
    if(initialSize == LARGE_BLOCK_PAGE_SIZE) {
        //printf("Freeing large block %p\n", GetLargeBlockPageFromPointer(ptr));
        FreeLargeBlockPage(GetLargeBlockPageFromPointer(ptr));
    } else {
        Page* page = GetPageFromPointer((void*)block);
        //PrintFreeList();
        //printf("Freeing block %p from page %p\n", block, page);
        //PrintPage(page);
        int coalescedSize;
        block = CoalesceBlock(block, initialSize, &coalescedSize);
        //printf("Adding block %p of size %i to free list\n", block, coalescedSize);
        AddBlockToFreeList(block, coalescedSize);
        page->numAllocatedBlocks--;
        //printf("Page %p has %i allocated blocks\n", page, page->numAllocatedBlocks);
        if(page->numAllocatedBlocks == 0 && page != (Page*)GetFirstPage()) {
            FreePage(page);
        }
        //PrintFreeList();
        AttemptToFreeFirstPage();
    }

    //getchar();
    //printf("\n\n\n");

}


#endif // KMA_BUD
