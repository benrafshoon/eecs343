#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "thread_pool.h"

/**
 *  @struct threadpool_task
 *  @brief the work struct
 *
 *  Feel free to make any modifications you want to the function prototypes and structs
 *
 *  @var function Pointer to the function that will perform the task.
 *  @var argument Argument to be passed to the function.
 */

typedef struct {
    void (*function)(void *);
    void *argument;
} threadpool_task_t;



struct threadpool_t {
  pthread_mutex_t lock;
  pthread_cond_t new_work;
  pthread_cond_t not_full;
  int exit;
  pthread_t *threads;
  threadpool_task_t *task_queue;
  int thread_count;
  int task_queue_size;
  int task_queue_head;
  int task_queue_tail;
};

typedef struct {
    threadpool_t* threadPool;
    int threadNumber;
} ThreadArgument;

/**
 * @function void *threadpool_work(void *threadpool)
 * @brief the worker thread
 * @param threadpool the pool which own the thread
 */
static void* thread_do_work(void *threadpool);
static inline int IsTaskQueueEmpty(threadpool_t* threadPool);
static int AddToTailOfQueue(threadpool_t* threadPool, void (* function)(void*), void* argument);
static void RemoveFromHeadOfQueue(threadpool_t* threadPool, threadpool_task_t* destination);



static inline int IsTaskQueueEmpty(threadpool_t* threadPool) {
    return threadPool->task_queue_head == -1;
}

static int AddToTailOfQueue(threadpool_t* threadPool, void (* function)(void*), void* argument) {
    int added = 0;
    printf("Add to tail\n");

    printf("Head %i, Tail %i, Size %i\n", threadPool->task_queue_head, threadPool->task_queue_tail, threadPool->task_queue_size);
    if(threadPool->task_queue_head != threadPool->task_queue_tail) {
        threadpool_task_t* newTask = &threadPool->task_queue[threadPool->task_queue_tail];
        newTask->function = function;
        newTask->argument = argument;


        if(threadPool->task_queue_head == -1) {
            threadPool->task_queue_head = threadPool->task_queue_tail;
        }

        threadPool->task_queue_tail++;
        if(threadPool->task_queue_tail >= threadPool->task_queue_size) {
            threadPool->task_queue_tail = 0;
        }
        added = 1;
        printf("New Head %i, Tail %i, Size %i\n", threadPool->task_queue_head, threadPool->task_queue_tail, threadPool->task_queue_size);
    } //else queue is full

    return added;

}

static void RemoveFromHeadOfQueue(threadpool_t* threadPool, threadpool_task_t* destination) {
    printf("Remove from head\n");
    printf("Head %i, Tail %i, Size %i\n", threadPool->task_queue_head, threadPool->task_queue_tail, threadPool->task_queue_size);
    threadpool_task_t* headTask = &threadPool->task_queue[threadPool->task_queue_head];

    destination->function = headTask->function;
    destination->argument = headTask->argument;

    threadPool->task_queue_head++;
    if(threadPool->task_queue_head == threadPool->task_queue_size) {
        threadPool->task_queue_head = 0;
    }

    //Queue is now empty
    if(threadPool->task_queue_head == threadPool->task_queue_tail) {
        threadPool->task_queue_head = -1;
    }
    printf("New Head %i, Tail %i, Size %i\n", threadPool->task_queue_head, threadPool->task_queue_tail, threadPool->task_queue_size);
}

/*
 * Create a threadpool, initialize variables, etc
 *
 */
threadpool_t *threadpool_create(int thread_count, int queue_size) {
    threadpool_t* threadPool = (threadpool_t*)malloc(sizeof(threadpool_t));
    threadPool->exit = 0;
    threadPool->threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);
    threadPool->task_queue = (threadpool_task_t*)malloc(sizeof(threadpool_task_t) * queue_size);
    threadPool->thread_count = thread_count;
    threadPool->task_queue_size = queue_size;
    threadPool->task_queue_head = -1;
    threadPool->task_queue_tail = 0;

    pthread_mutexattr_t mutexAttributes;
    pthread_mutexattr_init(&mutexAttributes);
    pthread_mutex_init(&threadPool->lock, &mutexAttributes);

    pthread_condattr_t conditionAttributes;
    pthread_condattr_init(&conditionAttributes);
    pthread_cond_init(&threadPool->new_work, &conditionAttributes);

    pthread_cond_init(&threadPool->not_full, &conditionAttributes);

    int threadNumber;
    for(threadNumber = 0; threadNumber < thread_count; threadNumber++) {
        pthread_attr_t threadAttributes;
        pthread_attr_init(&threadAttributes);
        ThreadArgument* arg = (ThreadArgument*)malloc(sizeof(ThreadArgument));
        arg->threadPool = threadPool;
        arg->threadNumber = threadNumber;
        pthread_create(&threadPool->threads[threadNumber], &threadAttributes, &thread_do_work, (void*)arg);
        printf("Created thread %i \n", threadNumber);
    }


    return threadPool;
}


/*
 * Add a task to the threadpool
 *
 */
int threadpool_add_task(threadpool_t *threadPool, void (* function)(void *), void *argument)
{
    int err = 0;

    /* Get the lock */
    pthread_mutex_lock(&threadPool->lock);

    /* Add task to queue */
    while(!AddToTailOfQueue(threadPool, function, argument)) {
        printf("Task queue full, waiting for room\n");
        pthread_cond_wait(&threadPool->not_full, &threadPool->lock);
    }
    printf("Added to queue\n");

    /* pthread_cond_broadcast and unlock */
    pthread_mutex_unlock(&threadPool->lock);

    pthread_cond_signal(&threadPool->new_work);

    return err;
}



/*
 * Destroy the threadpool, free all memory, destroy treads, etc
 *
 */
int threadpool_destroy(threadpool_t *threadPool)
{
    int err = 0;


    /* Wake up all worker threads */



    int threadNumber;
    /*for(threadNumber = 0; threadNumber < threadPool->thread_count; threadNumber++) {

        printf("Canceling thread %i\n", threadNumber);
        pthread_cancel(threadPool->threads[threadNumber]);

    }*/
    printf("\n\n\nShutting down threads\n");
    threadPool->exit = 1;

    pthread_cond_broadcast(&threadPool->new_work);

    for(threadNumber = 0; threadNumber < threadPool->thread_count; threadNumber++) {
        printf("About to wait on thread %i to cancel, testing lock\n", threadNumber);
        pthread_mutex_lock(&threadPool->lock);
        printf("Unlocked\n");
        pthread_mutex_unlock(&threadPool->lock);
        printf("Waiting on thread %i to cancel\n", threadNumber);
        void* retval;
        pthread_join(threadPool->threads[threadNumber], &retval);
        printf("Thread %i canceled\n", threadNumber);
    }

    /* Join all worker thread */

    printf("Freeing resources\n");
    pthread_mutex_destroy(&threadPool->lock);
    pthread_cond_destroy(&threadPool->new_work);
    pthread_cond_destroy(&threadPool->not_full);
    free(threadPool->threads);
    free(threadPool->task_queue);
    free(threadPool);

    /* Only if everything went well do we deallocate the pool */

    return err;
}



/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 *
 */
static void *thread_do_work(void *arg)
{
    ThreadArgument* threadArgument = (ThreadArgument*)arg;
    threadpool_t* threadPool = threadArgument->threadPool;
    int threadNumber = threadArgument->threadNumber;
    free(arg);
    threadpool_task_t currentTask;

    printf("Thread %i started\n", threadNumber);


    while(1) {
        printf("Thread %i waiting for access to queue\n", threadNumber);
        pthread_mutex_lock(&threadPool->lock);
        while(IsTaskQueueEmpty(threadPool)) {
            if(threadPool->exit) {
                printf("Thread %i exiting\n", threadNumber);
                pthread_mutex_unlock(&threadPool->lock);
                pthread_exit(NULL);
            } else {
                printf("Thread %i waiting for work, releasing access to queue\n", threadNumber);
                pthread_cond_wait(&threadPool->new_work, &threadPool->lock);
            }
        }

        printf("Thread %i starting work\n", threadNumber);
        RemoveFromHeadOfQueue(threadPool, &currentTask);
        pthread_mutex_unlock(&threadPool->lock);

        //Signal that the queue is not full
        printf("Signalling queue not full\n");
        pthread_cond_signal(&threadPool->not_full);

        currentTask.function(currentTask.argument);

        pthread_testcancel();
    }

    //Prevents the compiler from complaining, but again, this is an undefined state
    return NULL;
}
