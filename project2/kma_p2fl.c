/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the power-of-two free list
 *             algorithm
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
#ifdef KMA_P2FL
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/* #defines and typedefs should have their names in all caps.
* Global variables begin with g. Global constants with k. Local
* variables should be in all lower case. When initializing
* structures and arrays, line everything up in neat columns.
*/
#define LISTSIZE 10

typedef struct Buffer_
{
    void* head; //buffer header
    //void* next;
} Buffer;

typedef struct PageList_
{
    kma_page_t* page;
    struct PageList* next;
} PageList;

typedef struct FreeBufList_
{
    PageList* pagelist; //list of pages with ptr to start of list, list size and # of used pages
    Buffer* start;
    //PageList* next
    int blockSize;
    int numAllocatedBlocks;
} FreeBufList;

typedef struct MainBufList_ //array of free lists varying in blockSize from 32 to 8192
{
    FreeBufList FreeList[LISTSIZE];
    int numListsUsed;
} MainBufList;

/************Global Variables*********************************************/
kma_page_t* firstPageT = NULL;

/************Function Prototypes******************************************/
void InitializeFirstPage(kma_page_t* page);
void AddPage(kma_page_t* page, FreeBufList* list);
void AddBuffer(FreeBufList* list);
void* FindFit(MainBufList* list, int size);
void* GetBuffer(FreeBufList* list);
void FreeMainList();
void FreeBufferList(Buffer* buf, FreeBufList* list);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

void* FindFit(MainBufList* list, int size){
    //tests given size against available buffer sizes, and returns address of the best-fit list at FreeList[ndx]
    //returns NULL if size > 8KB
  int ndx = 0;
  if (size <= 32) {
     return &list->FreeList[ndx];
  } else {
     ndx++;
  }
  if (size > 32 && size <= 64) {
     return &list->FreeList[ndx];
  } else {
     ndx++;
  }
  if (size > 64 && size <= 128) {
     return &list->FreeList[ndx];
  } else {
     ndx++;
  }
  if (size > 128 && size <= 256) {
     return &list->FreeList[ndx];
  } else {
     ndx++;
  }
  if (size > 256 && size <= 512) {
      return &list->FreeList[ndx];
  } else {
      ndx++;
  }
  if (size > 512 && size <= 1024) {
      return &list->FreeList[ndx];
  }
  else {
      ndx++;
  }
  if (size > 1024 && size <= 2048) {
      return &list->FreeList[ndx];
  }
  else {
      ndx++;
  }
  if (size > 2048 && size <= 4096) {
      return &list->FreeList[ndx];
  } else {
      ndx++;
  }
  if (size > 4096 && size <= 8192) {
      return &list->FreeList[ndx];
  } else {
      return NULL;
  }
}

void*
kma_malloc(kma_size_t size)
{
  if(firstPageT == NULL){ //if we have no page to begin with, get one and initialize it
    firstPageT = get_page();
    InitializeFirstPage(firstPageT);
  }

  FreeBufList* req_ptr = NULL;
  void* res_ptr = NULL;

  MainBufList* list = (MainBufList*)firstPageT->ptr; //get main buffer list
  int adj_size = size + sizeof(Buffer); //account for size of buffer in request
  req_ptr = FindFit(list, adj_size); //find the right buffer list for the request

  if (req_ptr != NULL) //if we find the right-sized buffer, go get it
    res_ptr = GetBuffer(req_ptr);

  return res_ptr; //return pointer to newly alloc'd space
}


void
InitializeFirstPage(kma_page_t* page)
{

  MainBufList* mainlist = (MainBufList*)page->ptr;

  int i;
  int power = 5;
    for(i = 0; i < (LISTSIZE - 1); i++){ //set buffer size of each free list by increasing powers of two
        mainlist->FreeList[i].blockSize = (1 << power);
        power++;
    }

    for (i = 0; i < LISTSIZE; i++){ //initialize pointers to start and lists, set numAllocatedBlocks to 0
        mainlist->FreeList[i].start = NULL;
        mainlist->FreeList[i].pagelist = NULL;
        mainlist->FreeList[i].numAllocatedBlocks = 0;
    }

    int pagelistplusbuf = sizeof(PageList) + sizeof(Buffer); //set var for easy use later
    mainlist->FreeList[9].blockSize = pagelistplusbuf; //initialize blocksize and # of lists used
    mainlist->numListsUsed = 0;


    int bufcount = (PAGESIZE - sizeof(MainBufList)) / pagelistplusbuf; //get # of buffers that will fit on page
    void* start = page->ptr + sizeof(MainBufList); //set starting point
    Buffer* buffer_list_head;
    int j;
    for(j = 0; j < bufcount; j++){
        buffer_list_head = (Buffer*)(start + j*pagelistplusbuf); //create buffers and move start of list upward
        buffer_list_head->head = mainlist->FreeList[9].start;  //as new buffers are created
        mainlist->FreeList[9].start = buffer_list_head; //note the start of the list of used pages
    }

  AddPage(page, &mainlist->FreeList[9]); //add page to list of used pages
}


void
AddPage(kma_page_t* page, FreeBufList* list)
{
  MainBufList* mainbuf = (MainBufList*)firstPageT->ptr;
  void* newbuf = GetBuffer(&mainbuf->FreeList[9]); //get a buffer from sliced-up page
  PageList* newpagelist = (PageList*)newbuf; //set a new page list for that page
  newpagelist->page = page;

  newpagelist->next = (void*)list->pagelist; //make newly added page head of list and point to rest of list
  list->pagelist = newpagelist;
}

void*
GetBuffer(FreeBufList* list)
{
    if (list->start == NULL) { //add buffer if we have no list of them handy
        AddBuffer(list);
    }
    Buffer* buf = list->start;
    list->start = (Buffer*)buf->head; //mark new block as used and set start of list to next free block
    buf->head = (void*)list;
    list->numAllocatedBlocks++;
    MainBufList* bufmain = firstPageT->ptr;
    if(list != &bufmain->FreeList[9]) {
        bufmain->numListsUsed++; //increment the counter for the buffer list if it hasn't already been used
    }
    void* newbuf = (void*)buf + sizeof(Buffer);
    return newbuf; //return ptr to newly alloc'd block
}


void
AddBuffer(FreeBufList* list)
{
  kma_page_t* page = get_page(); //get a new page and slice it up according to its designated block size
  void* start = page->ptr;
  int i;
  for(i = 0; i < (PAGESIZE / list->blockSize); i++){ //for loop does the actual slicing and setting the list start ptr
    Buffer* buf = (Buffer*)(start + i*list->blockSize);
    buf->head = list->start;
    list->start = buf;
  }
  MainBufList* bufmain = (MainBufList*)firstPageT->ptr;
  PageList* newlist = (PageList*)GetBuffer(&bufmain->FreeList[9]); //tell the main list that we've added new buffers
  newlist->page = page;                                             //and mark this page as used

  newlist->next = (void*)list->pagelist;
  list->pagelist = newlist;
}

void FreeBufferList(Buffer* buf, FreeBufList* list){ //free the free list of blocks
  buf->head = list->start; //move the starting ptr 'down' one block in the list
  list->start = buf;
  list->numAllocatedBlocks--; //decrement number of alloc'd blocks

  if (list->numAllocatedBlocks == 0) { //if there aren't any blocks left in the list, wipe the free list clean
    list->start = NULL;
    PageList* page = list->pagelist;
    while (page != NULL) {
      free_page(page->page);
      page = (PageList*)page->next;
    }
    list->pagelist = NULL;
  }
}

void
kma_free(void* ptr, kma_size_t size)
{
  void* bufspace = (void*)ptr - sizeof(Buffer); //get wiping space minus metadata
  Buffer* freebuf = (Buffer*)bufspace;
  FreeBufList* list = (FreeBufList*)freebuf->head; //set new start of list, eliminating old bufspace
  FreeBufferList(freebuf, list); //check that bufscape is not the last block in an otherwise free list
  FreeMainList(); //free the entire main list when finished
}

void FreeMainList()
{
   MainBufList* list = (MainBufList*)firstPageT->ptr; //if list is totally un-alloc'd, decrement used list counter
   list->numListsUsed--;

   if (list->numListsUsed == 0) { //is every list totally un-alloc'd?
     PageList* page = list->FreeList[9].pagelist; //then free every page marked 'used' to make up the list!
     while (page->next != NULL) { //while loop traverses singly linked list to free all pages
       free_page(page->page);
       page = (PageList*)page->next;
     }
     free_page(page->page); //frees the very last page and sets very first page ptr to NULL--everything clean and free
     firstPageT = NULL;

   }

}
#endif // KMA_P2FL
