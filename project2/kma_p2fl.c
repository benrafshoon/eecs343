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

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define MINPOWER 32
#define MAXBUFSIZE 8192
#define FREELISTSIZE 10
#define PAGES 9

typedef struct Buf_{
    void* start;
} Buf;

typedef struct PageList_ {
    kma_page_t* new_page;
    struct PageList_* next;
} PageList;

typedef struct FreeList_{
    int size;
    int used;
    PageList* first;
    Buf* free_buf_list;
} FreeList;

typedef struct BufferList_{
    FreeList bufArray[10];
    int used;
} BufferList;

/************Global Variables*********************************************/
kma_page_t* init = NULL;


/************Function Prototypes******************************************/
void SetupPage(kma_page_t* page);
void InitBufArray(BufferList* list);
void* GetBuffer(FreeList* from_list);
void MakeBuffer(FreeList* from_list, void* ptr);
void SlicePage(void* ptr, BufferList* list, int count);
void AddPageToList(kma_page_t* page, FreeList* home_list);
void FreePage(FreeList* list);

/************External Declaration*****************************************/

/**************Implementation***********************************************/
void InitBufArray(BufferList* list){
    int i;
    int pow = 5;
    for(i = 0; i < (FREELISTSIZE - 1); i++){
        list->bufArray[i].size = 2 ^ pow;
        pow++;
        list->bufArray[i].first = NULL;
    }
    int j;
    for (j = 0; j < FREELISTSIZE; j++){
        list->bufArray[i].first = NULL;
        list->bufArray[i].used = 0;
        list->bufArray[i].free_buf_list = NULL;
    }
    list->bufArray[PAGES].size = sizeof(PageList) + sizeof(Buf);
    list->used = 0;


}

void SlicePage(void* ptr, BufferList* list, int count){
    Buf* header;
    int i;
    for (i = 0; i < count; i++){
        header = (Buf*)(ptr + i * (sizeof(PageList) + sizeof(Buf)));
        header->start = list->bufArray[PAGES].first;
    }
}

void SetupPage(kma_page_t* page)
{
    //initialize free list
    BufferList* buf_list = (BufferList*)page->ptr;
    InitBufArray(buf_list);
    int buf_count = (PAGESIZE - sizeof(buf_list)) / (sizeof(PageList) + sizeof(Buf));

    void* begin = page->ptr + sizeof(buf_list);
    SlicePage(begin, buf_list, buf_count);
    AddPageToList(page, &buf_list->bufArray[PAGES]);

}

void MakeBuffer(FreeList* from_list, void* ptr){
        int i;
        for (i = 0; i < (PAGESIZE / from_list->size); i++){
            void* newb = ptr + (i * from_list->size);
            Buf* new_buf = (Buf*)newb;
            new_buf->start = from_list->first;
            from_list->free_buf_list = new_buf;
        }
}

void AddPageToList(kma_page_t* page, FreeList* home_list){
    BufferList* list = (BufferList*)init->ptr;
    PageList* newPageList = (PageList*)GetBuffer(&list->bufArray[PAGES]);
    newPageList->new_page = page;
    newPageList->next = home_list->first;
    home_list->first = newPageList;

}

void* GetBuffer(FreeList* from_list){

    if (from_list->first == NULL){
        kma_page_t* page = get_page();
        void* p_start = page->ptr;
        MakeBuffer(from_list, p_start);
        AddPageToList(page, from_list);
    }
    Buf* newBuf = from_list->free_buf_list;
    from_list->free_buf_list = (Buf*)newBuf->start;
    newBuf->start = (void*)from_list;
    from_list->used += 1;
    BufferList* bufL = init->ptr;
    void* rest_buf = (void*)newBuf + sizeof(Buf);
    if (from_list == &bufL->bufArray[PAGES]){
        return rest_buf;
    } else {
        bufL->used +=1;
        return rest_buf;
    }
}


void*
kma_malloc(kma_size_t size)
{
    if (init == NULL){
        init = get_page();
        SetupPage(init);
    }


    int adj_size = size + sizeof(Buf);
    BufferList* list = (BufferList*)init->ptr;
    FreeList* req_mem = NULL;

    int ndx = 0;
    unsigned long long int bufsize = 1ULL << MINPOWER;
    assert(adj_size <= MAXBUFSIZE);
    while(bufsize < adj_size){
        ndx++;
        bufsize <<= 1;
    }
    req_mem = &list->bufArray[ndx];
    if (req_mem != NULL){
        void* result_ptr = GetBuffer(req_mem);
        return result_ptr;
    }
    else {
        return NULL;
    }

}

void FreePage(FreeList* list){
    PageList* plist = list->first;
    while (plist != NULL){
        free_page(plist->new_page);
        plist = plist->next;
    }
}

void
kma_free(void* ptr, kma_size_t size)
{
    void* contents = (void*)ptr - sizeof(Buf);
    Buf* bufContents = (Buf*)contents;
    FreeList* list = (FreeList*)bufContents->start;
    bufContents->start = list->free_buf_list;
    list->free_buf_list = bufContents;
    list->used -= 1;

    if (list->used == 0) {
        list->free_buf_list = NULL;
        FreePage(list);
        list->first = NULL;
    }

    BufferList* listFree = (BufferList*)init->ptr;
    listFree->used -= 1;
    if (listFree->used == 0){
        PageList* pList = listFree->bufArray[PAGES].first;
        while (pList->next != NULL){
            free_page(pList->new_page);
            pList = pList->next;
        }
        free_page(pList->new_page);
        init = NULL;
    }
}

#endif // KMA_P2FL

