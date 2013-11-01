/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the resource map algorithm
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
#ifdef KMA_RM
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

//The resource map is implemented as a singly linked list of <base, size> pairs

//Block entry in the list of free blocks
//Each entry implements both a linked list from the nextBlock pointer,
//and the <base, size> pair of the resource map using the address of the block as the base
//and the size field as the size
typedef struct Block_ {
    unsigned int size;
    struct Block_* nextBlock;
} Block;

//Header structure at the beginning of each page
//Contains a link back to the kma_page_t so that the page can be freed when not used
//Padding field added so that the size is MIN_BLOCK_SIZE
typedef struct Page_ {
    kma_page_t* pageT;
    void* padding;
} Page;

//Header structure at the beginning of the first page
//Contains a link back to the kma_page_t so that the page can be freed when not used
//Also contains the head to the linked list of free blocks
//Intentionally similar structure to type Page
typedef struct FirstPage_ {
    kma_page_t* pageT;
    Block* firstFreeBlock;
} FirstPage;



/************Global Variables*********************************************/
//Pointer to the first page allocated that contains the head of the linked list of
//free blocks
kma_page_t* firstPageT = NULL;


/************Function Prototypes******************************************/

//Returns the first page which contains the head to the free list
inline FirstPage* GetFirstPage();

//Returns the next greatest size aligned to 8.
//ie AlignToMinBlockSize(15) returns 16
//   AlignToMinBlockSize(16) returns 16
inline kma_size_t AlignToMinBlockSize(kma_size_t size);

//Returns the page that a pointer is contained in
inline Page* GetPageFromPointer(void* pointer);

//Returns and removes the first free block of at least size
//If the first block is bigger than size, then the block is split
//and the remaining size is placed back into the list of free blocks
Block* FirstFit(kma_size_t size);

//Allocates a new page
//Creates and returns block of size 8184
Block* AllocateNewPage();

//Adds a block to the list of free blocks
//Returns the block before where toAdd was inserted
Block* AddBlockToFreeList(Block* toAdd);

//Finds the position in the free list where block would fit
//Returns a pointer to the previous position block and next position block
//previousBlock and nextBlock must not be NULL
void GetAdjacentFreeBlocks(Block* block, Block** previousBlock, Block** nextBlock);

//Removes block from the free list
void RemoveBlockFromFreeList(Block* block);

//Initializes the first page, which initializes the free list
//with a block of size 8184
void InitializeFirstPage();

//If there are no allocated blocks and the only page left is the first page,
//frees the first page
void AttemptToFreeFirstPage();

/************External Declaration*****************************************/

/**************Implementation***********************************************/


inline Page* GetPageFromPointer(void* pointer) {
    //Clear the least significant 12 bits
    return (Page*)((size_t)pointer & ~0x1FFF);
}

inline kma_size_t AlignToMinBlockSize(kma_size_t size) {
    return (size + (MIN_BLOCK_SIZE - 1)) & ~(MIN_BLOCK_SIZE - 1);
}

inline FirstPage* GetFirstPage() {
    return (FirstPage*)firstPageT->ptr;
}

Block* AddBlockToFreeList(Block* toAdd) {
    FirstPage* firstPage = GetFirstPage();

    Block* nextBlock = firstPage->firstFreeBlock;
    Block* previousBlock = NULL;
    //Search linked list to find location to add
    //previousBlock will be the location before insertion
    //nextBlock will be the location after insertion
    while(nextBlock != NULL && (size_t)nextBlock < (size_t)toAdd) {
        previousBlock = nextBlock;
        nextBlock = nextBlock->nextBlock;
    }

    toAdd->nextBlock = nextBlock;

    //If previousBlock is null, we inserted toAdd before the first block on the linked list
    if(previousBlock == NULL) {
        //update the head pointer to point to our inserted block
        firstPage->firstFreeBlock = toAdd;
    } else {
        //Otherwise maintain the linked list structure
        previousBlock->nextBlock = toAdd;
    }
    //Return the previous block so we can maintain the linked list structure
    //if we split the block
    return previousBlock;
}

Block* FirstFit(kma_size_t size) {

    FirstPage* firstPage = GetFirstPage();
    Block* currentBlock = firstPage->firstFreeBlock;
    Block* previousBlock = NULL;

    //Find first block of at least size
    while(currentBlock != NULL && currentBlock->size < size) {
        previousBlock = currentBlock;
        currentBlock = currentBlock->nextBlock;
    }

    //If we couldn't find a big enough block, allocate a new page
    if(currentBlock == NULL) {
        //Allocate a page and get the block from the newly allocated page
        currentBlock = AllocateNewPage();

        //Insert the block to the free list and find the block previous
        //to the place of insertion
        previousBlock = AddBlockToFreeList(currentBlock);
    }

    //nextBlock either points to the block after the one we found
    //Or the righthand section of the block, if we split the block
    Block* nextBlock;

    //If the block we found is too big, split it
    if(currentBlock->size > size) {
        //Find the new size of the righthand section
        kma_size_t newSize = currentBlock->size - size;
        //Get the location of the righthand section
        nextBlock = (Block*)((size_t)currentBlock + size);
        nextBlock->size = newSize;
        //Maintain the linked list
        nextBlock->nextBlock = currentBlock->nextBlock;
    } else {
        nextBlock = currentBlock->nextBlock;
    }

    //If the previousBlock is null, we removed the head of the linked list
    if(previousBlock == NULL) {
        //Update the head with the next block
        firstPage->firstFreeBlock = nextBlock;
    } else {
        //Maintain the linked list when we remove currentBlock
        previousBlock->nextBlock = nextBlock;
    }
    return currentBlock;
}

void GetAdjacentFreeBlocks(Block* block, Block** previousBlock, Block** nextBlock) {
    FirstPage* firstPage = GetFirstPage();
    *nextBlock = firstPage->firstFreeBlock;
    *previousBlock = NULL;
    //Search the linked list for where block would go if it was inserted
    //previousBlock will point to the place before the insertion point
    //nextBlock will point to the place after the insertion point
    while(*nextBlock != NULL && (size_t)(*nextBlock) < (size_t)block) {
        *previousBlock = *nextBlock;
        *nextBlock = (*nextBlock)->nextBlock;
    }
}

void RemoveBlockFromFreeList(Block* block) {
    FirstPage* firstPage = GetFirstPage();
    Block* currentBlock = firstPage->firstFreeBlock;
    Block* previousBlock = NULL;

    //Find the block in the list
    while(currentBlock != NULL && currentBlock != block) {
        previousBlock = currentBlock;
        currentBlock = currentBlock->nextBlock;
    }

    //If we found it, perform a linked list delete
    if(currentBlock != NULL) {
        if(previousBlock == NULL) {
            firstPage->firstFreeBlock = currentBlock->nextBlock;
        } else {
            previousBlock->nextBlock = currentBlock->nextBlock;
        }
    }
}

Block* AllocateNewPage() {
    kma_page_t* newPageT = get_page();
    Page* newPage = (Page*)newPageT->ptr;
    newPage->pageT = newPageT;
    Block* newPageBlock = (Block*)((size_t)newPage + sizeof(Page));
    newPageBlock->size = PAGESIZE - sizeof(Page);
    return newPageBlock;
}

void InitializeFirstPage() {
    if(firstPageT == NULL) {
        firstPageT = get_page();
        FirstPage* firstPage = (FirstPage*)firstPageT->ptr;
        firstPage->pageT = firstPageT;
        Block* firstPageBlock = (Block*)((size_t)firstPage + sizeof(FirstPage));
        firstPageBlock->nextBlock = NULL;
        firstPageBlock->size = PAGESIZE - sizeof(FirstPage);
        firstPage->firstFreeBlock = firstPageBlock;
    }
}

void AttemptToFreeFirstPage() {
    //If the first page is the only allocated page,
    //and the only block on the first page is the size of the entire page
    //then free the first page
    FirstPage* firstPage = GetFirstPage();
    kma_page_stat_t* currentPageStat = page_stats();
    if(currentPageStat->num_in_use == 1 && firstPage->firstFreeBlock->size == PAGESIZE - sizeof(Page) && GetPageFromPointer(firstPage->firstFreeBlock) == (Page*)firstPage) {
        free_page(firstPageT);
        firstPageT = NULL;
    }
}

void* kma_malloc(kma_size_t size) {
    InitializeFirstPage();

    size = AlignToMinBlockSize(size);
    Block* block = FirstFit(size);
    return (void*)block;
}


void kma_free(void* ptr, kma_size_t size) {
    FirstPage* firstPage = GetFirstPage();

    Block* block = (Block*)ptr;
    size = AlignToMinBlockSize(size);
    block->size = size;

    Block* nextBlock;
    Block* previousBlock;
    GetAdjacentFreeBlocks(block, &previousBlock, &nextBlock);

    //If there is a free block before the block we are freeing
    if(previousBlock != NULL) {
        //If the previousBlock is adjacent to the block we are freeing, coalesce
        if((Block*)((size_t)previousBlock + previousBlock->size) == block) {
            previousBlock->size = previousBlock->size + block->size;
            block = previousBlock;
        } else {
            //Otherwise, just perform a linked list insert
            block->nextBlock = previousBlock->nextBlock;
            previousBlock->nextBlock = block;

        }
    }

    //If there is a free block after the block we are freeing
    if(nextBlock != NULL) {
        //If the nextBlock is adjacent to the block we are freeing, coalesce
        if((Block*)((size_t)block + block->size) == nextBlock) {
            block->size = block->size + nextBlock->size;
            block->nextBlock = nextBlock->nextBlock;
        } else {
            //Otherwise, just perform a linked list insert
            block->nextBlock = nextBlock;
        }
        //If we inserted the free block before the first block in the linked list
        //The newly freed block is now the first block
        if(nextBlock == firstPage->firstFreeBlock) {
            firstPage->firstFreeBlock = block;
        }
    }

    //If previously the list of free blocks was empty,
    //update the first entry in the list with the newly freed block
    if(firstPage->firstFreeBlock == NULL) {
        firstPage->firstFreeBlock = block;
    }

    //If after coalescing, the block we freed takes up the entire page, and that page is not the first page
    //free the page and remove the block in that page from the list of free blocks
    Page* page = GetPageFromPointer(block);
    if(block->size == PAGESIZE - sizeof(Page) && page != (Page*)firstPage) {
        RemoveBlockFromFreeList(block);
        free_page(page->pageT);
    }

    //Attempt to free the first page, if possible
    AttemptToFreeFirstPage();

}

#endif // KMA_RM
