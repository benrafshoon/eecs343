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

 //Define constants
 #define FREE 0
 #define ALLOCATED 1
 #define MIN_BLOCK_SIZE 20

//Define our block structure which we split up to allocate memory
typedef struct Block_ {
    void* base; //where the block starts
    unsigned int size; //size of the block
    int status; //whether or not it's free
    void* next_block; //pointer to the next block
    void* previous_block; //pointer to the previous block
} Block;



/************Global Variables*********************************************/
//Pull a first page and create a first block from it
kma_page_t* firstPageT = NULL; //a first page
Block* first_blockT= NULL; //a first block that is essentially a page to be divided up


/************Function Prototypes******************************************/
//initialize a block from a page
Block* initialize_block (kma_page_t* Page);
//find the first fit and return its block pointer then split the block up
void* first_fit(kma_size_t size, Block* first_blockT);
//split the block up
void split_block(Block* block, kma_size_t size, void* base);
//add a new page as a block onto our map of blocks
Block* add_page(Block* block, kma_page_t* Page);
//free a (portion of) a block
void free_block(Block* block,kma_size_t size,void* base);
//coalesce that portion
void coalesce_block(Block* block);
//free any unused pages
void free_pages(Block* first_blockT);


/************External Declaration*****************************************/

/**************Implementation***********************************************/
Block* initialize_block(kma_page_t* Page){
    Block* first_block; //set all of the block's properties as the page
    first_block=Page->ptr;
    first_block->size=Page->size;
    first_block->status=FREE;
    first_block->next_block=NULL;
    first_block->previous_block=NULL;
    return first_block;
    }

void* first_fit(kma_size_t size, Block* first_blockT){
    Block* block;
    block=first_blockT; //start at the first block
    while(block->next_block!=NULL){ //loop through until you run out of blocks
        if(size < (block->size-sizeof(Block*)) && block->status==FREE){ //if the needed size will fit in the block along with meta data...
            void* base;
            base = block->base; //set up to take the needed chunk
            split_block(block, size, base);
            return base;
        }
        else{block=block->next_block;}
    }
    kma_page_t* new_page; //if we need a new page get one
    new_page=get_page();
    Block* new_block;
    new_block=add_page(block, new_page); //make a block from that page
    void* base=new_block->base;
    split_block(new_block, size, base);
    return base;
}

void split_block(Block* block, kma_size_t size, void* base){
            Block* new_block=block+sizeof(Block*)+size; //create a new block at the end of the newly allocated space
            new_block->base=block->base+sizeof(Block*)+size; //the base of this block is end of what's taken up
            new_block->size=block->size-size-sizeof(Block); //the size of this block is what's free of the last
            new_block->status=FREE;
            new_block->next_block=block->next_block;
            new_block->previous_block=block->base;
            // add a pointer to the block structure at the beginning of the block
            *((Block**)new_block->base) = new_block;
            block->size=size+sizeof(Block*); //the size of the split block is the space needed and the meta data
            block->status=ALLOCATED;
            block->next_block=base+block->size;
}

Block* add_page(Block* block, kma_page_t* Page){
    Block* new_block=Page->ptr; //make a block at the location of the newly created page
    new_block->base=Page->ptr; //get values off that page into a block
    new_block->size=Page->size;
    new_block->status=FREE;
    new_block->next_block=NULL;
    new_block->previous_block=block->base;
    // add a pointer to the block structure at the beginning of the block
    *((Block**)new_block->base) = new_block;
    block->next_block=new_block->base;
    return new_block;
}

void free_block(Block* block,kma_size_t size,void* base){ //make the block free and try to coalesce
    block->status=FREE;
    coalesce_block(block);
    }

void coalesce_block(Block* block){
    //check next block
    Block* next_block;
    next_block=block->next_block;
    if(next_block->status==FREE){
        block->size=block->size+next_block->size;
        block->next_block=next_block->next_block;
    }
    //check previous block
    Block* previous_block;
    previous_block=block->previous_block;
    if(previous_block->status==FREE){
        previous_block->size=previous_block->size+block->size;
        previous_block->next_block=block->next_block;
    }
}

void free_pages(Block* first_blockT){
    int id=0; //initialize at the first block/page
    Block* block;
    block=first_blockT;
    while((block->base+firstPageT->size)!=NULL){ //run through all pages
        if(block->size==firstPageT->size && block->status==FREE){ //if the block is a page and FREE...
            kma_page_t* page=block->base; //convert the block to a page at the loaction of the block
            page->id=id;
            page->ptr=block->base;
            page->size=firstPageT->size;
            free_page(page); //and free that page
            }
        else{ //increment through all pages by address and ID
        block=block->base+firstPageT->size;
        id++;
        }
    }
 return;
}

void*
kma_malloc(kma_size_t size)
{
    if(firstPageT==NULL){
        firstPageT=get_page();
        first_blockT=initialize_block(firstPageT);
        // add a pointer to the block structure at the beginning of the block
        *((Block**)first_blockT->base) = first_blockT;
    }
    if ((size + sizeof(Block*)) > PAGESIZE) //if the requested size along with the meta-data exceeds a page...
    { // requested size too large
      return NULL;
    }

    else{
    void* base; //get the base of the first fit page
    base=first_fit(size, first_blockT);
    return base+sizeof(Block*); //return the address where the available memory starts
    }

}

void
kma_free(void* ptr, kma_size_t size)
{
  Block* block; //loop through like before to find the desired block
  block=first_blockT;
    while(block->next_block!=NULL){
        if((block->base+sizeof(Block*))==ptr && block->status==ALLOCATED){
            void* base;
            base = block->base;
            free_block(block, size, base); //free the block when found
            break;
        }
        else{block=block->next_block;}
    }
    free_pages(first_blockT); //after freeing and coalescing blocks attempt to free any unused pages
}

#endif // KMA_RM
