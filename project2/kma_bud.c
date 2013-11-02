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

//Large block pages are pages where the block is larger than 4096 bytes
//So the block takes up the entire page
//LARGE_BLOCK_PAGE_SIZE_INDEX = Log2(8192) - Log2(8)
 #define LARGE_BLOCK_PAGE_SIZE_INDEX 10

//Min block size 8 bytes
//FREELIST_SIZE = log2(4096) - log2(8) + 1
#define FREELIST_SIZE 10

//Blocks are a singly linked list of free blocks
typedef struct Block_ {
    struct Block_* nextBlock;
} Block;

//AHeader for each page, containing the kma_page_t and the number of allocated blocks
//so that the page can be deallocated when numAllocatedBlocks goes to 0
//The header is not counted as an allocated block
typedef struct Page_ {
    kma_page_t* page_t;
    int numAllocatedBlocks;
} Page;

//A LargeBlockPage is a block that can take up the entire page
//No need to keep track of the number of allocated blocks because the entire page is one block
typedef struct LargeBlockPage_ {
    kma_page_t* page_t;
} LargeBlockPage;



//Header for the first page
//Contains the power of 2 lists of free blocks
//Like Page, also contains a pointer to the kma_page_t and the number of allocated blocks for page deallocation
//The header is not counted as an allocated block
typedef struct FirstPage_ {
    kma_page_t* page_t;
    int numAllocatedBlocks;
    Block* freeList[FREELIST_SIZE];
    //freeList[0] contains 8B blocks (the minimum block size)
    //freeList[1] contains 16B blocks
    //...
    //freeList[9] contains 4KB blocks
    //8KB blocks are special cases because a new page is always allocated for 8KB blocks
} FirstPage;

/************Global Variables*********************************************/

//Pointer to the first page that contains the free list heads
static kma_page_t* firstPageT = NULL;

/************Function Prototypes******************************************/
//NOTE - All size parameters, except in malloc and free, are Log2(size) - 3, the index into the freelist

//Returns the buddy address of a block, given the size of the block
inline Block* GetBuddy(Block* block, int blockSize);

//Returns the page that a pointer is contained in
inline Page* GetPageFromPointer(void* pointer);

//Returns the pointer to the beginning of the usable segment of a LargeBlockPage
inline void* GetPointerFromLargeBlockPage(LargeBlockPage* largeBlockPage);

//Returns the LargeBlockPage associated with a pointer from a free with pagesize > 4096
inline LargeBlockPage* GetLargeBlockPageFromPointer(void* pointer);

//Returns the first page, which contains the free list heads
inline FirstPage* GetFirstPage();

//Returns the next power of 2 of a number
inline unsigned int NextPowerOf2(unsigned int number);

//Returns the log base 2 of a number
inline unsigned int Log2(unsigned int number);

//Returns the lower addressed block
inline Block* LowerBlock(Block* block1, Block* block2);

//Splits a block into the requested size, by repeatedly splitting the block in half
//and putting the right half into the list of free blocks
Block* SplitFreeBlock(Block* block, int requestedSize, int currentSize);

//Initializes the first page, which contains the power of 2 free list heads
void InitializeFirstPage();

//Inserts a block into the head of the size-appropriate free list
//O(1) operation
void AddBlockToFreeList(Block* toAdd, int size);

//Removes a block from the size-appropriate free list
//Returns the block if it was removed, and NULL if the block is not in the free list
//O(n) operation since we need to traverse the entire free list of the appropriate size
//for the block
Block* RemoveBlockFromFreeList(Block* toRemove, int size);

//Searches the power of 2 free lists starting with blocks of the requestedSize
//If no blocks of the requested size are available, it will attempt to find the next smallest size of
//block big enough.  Returns the found block (does not split it), and places the size of the
//found block into actualSize, which must not be null
//O(1) with respect to the size of any free list
Block* FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(int requestedSize, int* actualSize);

//Allocates a new page and places the new free blocks of that page in the free list
void AllocatePage();

//Recursively attempts to coalesce a block with its buddies
//Returns the coalesced block and places the size of the coalesced block in coalescedSize,
//which must not be null
Block* CoalesceBlock(Block* block, int initialSize, int* coalescedSize);

//Frees a page
void FreePage(Page* page);

//Frees the first page if the first page is the only page left
//and there are no allocated blocks in the first page
void AttemptToFreeFirstPage();

//Allocates a block larger than 4096 bytes, which requires allocating a new page
LargeBlockPage* AllocateLargeBlockPage();

//Frees a block larger than 4096, which is a block that takes up an entire page
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

Block* SplitFreeBlock(Block* block, int requestedSize, int currentSize) {
    Block* currentBlock = block;
    while(currentSize > requestedSize) {
        //New size is half the previous size
        //-1 instead of /2 or >>1 because we are working with the Log2 of the size
        int newSize = currentSize - 1;
        Block* leftBlock = block;
        //8 << newSize returns the int size from the Log2 size
        Block* rightBlock = (Block*)((size_t)block + (8 << newSize));
        //Each right half is added to the free list
        AddBlockToFreeList((Block*)rightBlock, newSize);
        currentBlock = leftBlock;
        currentSize = newSize;
    }
    return currentBlock;
}

void AddBlockToFreeList(Block* toAdd, int size) {
    FirstPage* firstPage = GetFirstPage();
    //Linked list insert to head of list
    toAdd->nextBlock = firstPage->freeList[size];
    firstPage->freeList[size] = toAdd;
}

Block* RemoveBlockFromFreeList(Block* toRemove, int size) {
    FirstPage* firstPage = GetFirstPage();
    Block* block = firstPage->freeList[size];
    Block* previousBlock = NULL;
    //Search list to find block
    while(block != NULL && block != toRemove) {
        previousBlock = block;
        block = block->nextBlock;
    }
    //If we can't find it in the list, return NULL
    if(block == NULL) {
        return NULL;
    }
    //If we're removing the head of the list, replace the head with the next block
    if(firstPage->freeList[size] == toRemove) {
        firstPage->freeList[size] = toRemove->nextBlock;
    }
    //Update the previousBlock to maintain the list structure
    if(previousBlock != NULL) {
        previousBlock->nextBlock = toRemove->nextBlock;
    }
    return toRemove;
}

void InitializeFirstPage() {
    if(firstPageT == NULL) {
        //Allocate a new page
        firstPageT = get_page();
        FirstPage* firstPage = GetFirstPage();
        firstPage->numAllocatedBlocks = 0;
        int i;
        for(i = 0; i < FREELIST_SIZE; i++) {
            firstPage->freeList[i] = NULL;
        }
        //Ensure that the header is an allocated block, and place the remaining blocks in the free list
        //3 is the Log2 of the next power of 2 of the first page header
        SplitFreeBlock((Block*)firstPage, 3, LARGE_BLOCK_PAGE_SIZE_INDEX);
    }
}


Block* FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(int requestedSize, int* actualSize) {
    FirstPage* firstPage = GetFirstPage();
    Block* block = NULL;
    //Loop through the power of 2 lists, starting with the requested size,
    //but search larger sizes if necessary
    int size;
    for(size = requestedSize; size < FREELIST_SIZE; size++) {
        block = firstPage->freeList[size];
        if(block != NULL) {
            firstPage->freeList[size] = block->nextBlock;
            break;
        }
    }
    *actualSize = size;
    return block;
}

void AllocatePage() {
    //Allocate a new page
    kma_page_t* page_t = get_page();
    Page* page = (Page*)page_t->ptr;
    page->page_t = page_t;
    page->numAllocatedBlocks = 0;
    //Ensure the header is an allocated block, and place the remaining blocks in the free list
    SplitFreeBlock((Block*)page, 0, LARGE_BLOCK_PAGE_SIZE_INDEX);
}

Block* CoalesceBlock(Block* block, int initialSize, int* coalescedSize) {
    //At each power of 2 size starting with the initial size, attempt to coalesce
    //the block with its buddy
    int size = initialSize;
    Block* buddy = GetBuddy(block, size);
    while(size < LARGE_BLOCK_PAGE_SIZE_INDEX && RemoveBlockFromFreeList(buddy, size) != NULL) {
        Block* leftBlock = LowerBlock(block, buddy);
        size++;
        block = leftBlock;
        buddy = GetBuddy(block, size);
    }
    *coalescedSize = size;
    return block;
}

void FreePage(Page* page) {
    Block* pageBlock = (Block*)page;
    int coalescedSize;
    //Calling coalesce block with the page header will remove all the blocks
    //in this page from the free list, coalescing them into one block
    //of PAGESIZE, which is not added into the free list
    //This has the effect of removing all blocks in this page from the freelist
    CoalesceBlock(pageBlock, 0, &coalescedSize);
    free_page(page->page_t);
}

void AttemptToFreeFirstPage() {
    FirstPage* firstPage = GetFirstPage();
    kma_page_stat_t* currentPageStats = page_stats();
    //If the first page is the only page, and it has no allocated blocks, then free it
    if(firstPage->numAllocatedBlocks == 0 && currentPageStats->num_in_use == 1) {
        free_page(firstPageT);
        firstPageT = NULL;
    }
}

LargeBlockPage* AllocateLargeBlockPage() {
    kma_page_t* page_t = get_page();
    LargeBlockPage* largeBlockPage = (LargeBlockPage*)page_t->ptr;
    largeBlockPage->page_t = page_t;
    //No need to deal with marking the header as allocated since large block pages are never
    //added to the free list
    return largeBlockPage;
}

void FreeLargeBlockPage(LargeBlockPage* block) {
    free_page(block->page_t);
}

void* kma_malloc(kma_size_t size) {
    InitializeFirstPage();

    //Compute the size index
    int blockSize = Log2(NextPowerOf2(size)) - 3;

    if(blockSize == LARGE_BLOCK_PAGE_SIZE_INDEX) {
        LargeBlockPage* largeBlockPage = AllocateLargeBlockPage();
        return GetPointerFromLargeBlockPage(largeBlockPage);
    }

    //Try to find a big enough block; if we fail, allocate a new page and try again
    //We are certain to find one on the second try
    int actualSize;
    Block* toAllocate = FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(blockSize, &actualSize);
    if(toAllocate == NULL) {
        AllocatePage();
        toAllocate = FindSmallestBlockOfAtLeastSizeAndRemoveFromFreeList(blockSize, &actualSize);
    }
    //Split the found block into the size we requested
    toAllocate = SplitFreeBlock(toAllocate, blockSize, actualSize);

    //Increment the allocated block counter on the appropriate page
    Page* pageOfBlockToAllocate = GetPageFromPointer(toAllocate);
    pageOfBlockToAllocate->numAllocatedBlocks++;

    return toAllocate;
}


void kma_free(void* ptr, kma_size_t size) {
    Block* block = (Block*)ptr;
    int initialSize = Log2(NextPowerOf2(size)) - 3;

    if(initialSize == LARGE_BLOCK_PAGE_SIZE_INDEX) {
        FreeLargeBlockPage(GetLargeBlockPageFromPointer(ptr));
    } else {
        //Decrement the allocated block counter on the appropriate page
        Page* page = GetPageFromPointer(block);
        page->numAllocatedBlocks--;

        //Attempt to coalesce the block, and add the coalesced block
        //back to the free list
        int coalescedSize;
        block = CoalesceBlock(block, initialSize, &coalescedSize);
        AddBlockToFreeList(block, coalescedSize);

        //If there are no allocated blocks on the page, free the page
        if(page->numAllocatedBlocks == 0 && page != (Page*)GetFirstPage()) {
            FreePage(page);
        }

        AttemptToFreeFirstPage();
    }
}


#endif // KMA_BUD
