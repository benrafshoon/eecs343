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

/*
Valgrind gave us similar output to this:
http://stackoverflow.com/questions/13132890/leaking-memory-with-pthreads
We believe that the 4 non-free'd allocs are due to the pthread library, not our code,
similar to what was suggested in the thread.
*/

typedef struct {
    void (*function)(int);
    int argument;
} threadpool_task_t;



//The task queue is implemented as a moving fixed size queue.
//Both the head and the tail move as tasks are removed and added, respectively
//The head can be after the tail, this means that entries wrap around the high-numbered side of the array
//head == -1 indicates an empty queue
//head == tail indicates a full queue
//When a task is added to the queue, tail is incremented
//When a task is removed from the queue, head is incremented

struct threadpool_t {
  pthread_mutex_t lock; //Lock so that only one thread can modify the queue at a time
  pthread_cond_t new_work; //Condition signaled when the queue becomes non-empty
  pthread_cond_t not_full; //Condition signaled when the queue becomes non-full
  int exit; //Set this to true to indicate that all threads should terminate.  Does not terminate threads instantly.
  pthread_t *threads; //Array of thread ids
  threadpool_task_t *task_queue; //The queue of tasks to work on
  int thread_count; //Number of threads
  int task_queue_size; //Max size of the queue
  int task_queue_head; //Moving head and tail of the queue
  int task_queue_tail;
};

/**
 * @function void *threadpool_work(void *threadPoolArg)
 * @brief the worker thread
 * @param threadPoolArg the pool which own the thread
 */

//"Main" function for thread pool threads.  Threads are passed the threadpool that they belong to
static void* thread_do_work(void *threadPoolArg);

//Returns true if the task queue is empty, false otherwise.
static inline int IsTaskQueueEmpty(threadpool_t* threadPool);

//Adds a task to the tail end of the thread pool.  Returns true if the task was added, false if there wasn't room
//NOT THREAD SAFE.  Must be synchronized externally
static inline int AddToTailOfQueue(threadpool_t* threadPool, void (* function)(int), int argument);

//Removes a task from the head end (lower numbered) of the thread pool.  DOES NOT CHECK IF THERE IS A TASK TO GET
//Behavior is undefined if the queue is empty; call IsTaskQueueEmpty first to be sure
//Destination must be a valid, non-null threadpool_task_t
//The task that is removed is copied to destination
//NOT THREAD SAFE.  Must be synchronized externally
static inline void RemoveFromHeadOfQueue(threadpool_t* threadPool, threadpool_task_t* destination);



static inline int IsTaskQueueEmpty(threadpool_t* threadPool) {
    //-1 indicates empty.  We can't just check if head == tail because that would be true for both full and empty
    return threadPool->task_queue_head == -1;
}

static inline int AddToTailOfQueue(threadpool_t* threadPool, void (* function)(int), int argument) {
    int added = 0;

    //Check if there's room
    if(threadPool->task_queue_head != threadPool->task_queue_tail) {
        //Get the location in the queue of the next task to be added
        threadpool_task_t* newTask = &threadPool->task_queue[threadPool->task_queue_tail];

        //Copy the function and argument to the queue
        newTask->function = function;
        newTask->argument = argument;

        //If the queue was previously empty, mark that it is no longer empty
        if(threadPool->task_queue_head == -1) {
            threadPool->task_queue_head = threadPool->task_queue_tail;
        }

        //Increment the tail, and wrap around to 0 if we went past the right side
        threadPool->task_queue_tail++;
        if(threadPool->task_queue_tail >= threadPool->task_queue_size) {
            threadPool->task_queue_tail = 0;
        }
        added = 1;
    } //else queue is full

    return added;
}

static inline void RemoveFromHeadOfQueue(threadpool_t* threadPool, threadpool_task_t* destination) {

    //Get the task at the head of the list
    threadpool_task_t* headTask = &threadPool->task_queue[threadPool->task_queue_head];

    //Copy the function and argument to the destination
    destination->function = headTask->function;
    destination->argument = headTask->argument;

    //Increment the head pointer and wrap around to zero if we overflow the right end
    threadPool->task_queue_head++;
    if(threadPool->task_queue_head == threadPool->task_queue_size) {
        threadPool->task_queue_head = 0;
    }

    //If the queue is now empty, set head to -1
    if(threadPool->task_queue_head == threadPool->task_queue_tail) {
        threadPool->task_queue_head = -1;
    }
}

/*
 * Create a threadpool, initialize variables, etc
 *
 */
threadpool_t *threadpool_create(int thread_count, int queue_size) {
    threadpool_t* threadPool = (threadpool_t*)malloc(sizeof(threadpool_t));

    //Don't exit yet (we just started)
    threadPool->exit = 0;

    //Allocate space to store the thread ID of each thread
    threadPool->threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);

    //Allocate space for the task queue, and initialize it to be empty
    threadPool->task_queue = (threadpool_task_t*)malloc(sizeof(threadpool_task_t) * queue_size);
    threadPool->thread_count = thread_count;
    threadPool->task_queue_size = queue_size;
    threadPool->task_queue_head = -1;
    threadPool->task_queue_tail = 0;

    //Initialize the queue lock
    pthread_mutexattr_t mutexAttributes;
    pthread_mutexattr_init(&mutexAttributes);
    pthread_mutex_init(&threadPool->lock, &mutexAttributes);
    pthread_mutexattr_destroy(&mutexAttributes);

    //Initialize the empty and full conditions (producer-consumer problem)
    pthread_condattr_t conditionAttributes;
    pthread_condattr_init(&conditionAttributes);
    pthread_cond_init(&threadPool->new_work, &conditionAttributes);

    pthread_cond_init(&threadPool->not_full, &conditionAttributes);

    pthread_condattr_destroy(&conditionAttributes);

    //Start each thread
    int threadNumber;
    for(threadNumber = 0; threadNumber < thread_count; threadNumber++) {
        pthread_attr_t threadAttributes;
        pthread_attr_init(&threadAttributes);
        pthread_create(&threadPool->threads[threadNumber], &threadAttributes, &thread_do_work, (void*)threadPool);

        pthread_attr_destroy(&threadAttributes);
    }

    return threadPool;
}


/*
 * Add a task to the threadpool
 *
 */
int threadpool_add_task(threadpool_t *threadPool, void (* function)(int), int argument)
{
    int err = 0;

    /* Get the lock */
    pthread_mutex_lock(&threadPool->lock);

    /* Add task to queue */
    while(!AddToTailOfQueue(threadPool, function, argument)) {
        pthread_cond_wait(&threadPool->not_full, &threadPool->lock);
    }

    /* pthread_cond_broadcast and unlock */
    pthread_mutex_unlock(&threadPool->lock);

    pthread_cond_signal(&threadPool->new_work);

    return err;
}



/*
 * Destroy the threadpool, free all memory, destroy treads, etc
 * Blocks until all threads finish whatever job they were already working on and terminate
 */
int threadpool_destroy(threadpool_t *threadPool)
{
    int err = 0;

    threadPool->exit = 1;

    /* Wake up all worker threads */
    pthread_cond_broadcast(&threadPool->new_work);

    /* Join all worker thread */
    int threadNumber;
    for(threadNumber = 0; threadNumber < threadPool->thread_count; threadNumber++) {
        void* retval;
        //Wait for all threads to finish (will not interrupt running jobs, waits for all jobs to finish)
        pthread_join(threadPool->threads[threadNumber], &retval);
    }

    /* Only if everything went well do we deallocate the pool */

    pthread_mutex_destroy(&threadPool->lock);
    pthread_cond_destroy(&threadPool->new_work);
    pthread_cond_destroy(&threadPool->not_full);

    free(threadPool->threads);
    free(threadPool->task_queue);
    free(threadPool);

    return err;
}



/*
 * Work loop for threads. Should be passed into the pthread_create() method.
 *
 */
static void *thread_do_work(void *threadPoolArg)
{
    threadpool_t* threadPool = (threadpool_t*)threadPoolArg;

    threadpool_task_t currentTask;

    while(1) {
        //Acquire the lock to get access to modify the queue
        pthread_mutex_lock(&threadPool->lock);
        //Wait for work
        while(IsTaskQueueEmpty(threadPool)) {
            //Exit here if we're shutting down the server (this ensures that all jobs finish, but no new work is done)
            if(threadPool->exit) {
                pthread_mutex_unlock(&threadPool->lock);
                pthread_exit(NULL);
            } else {
                //Wait for work if the task queue is empty
                pthread_cond_wait(&threadPool->new_work, &threadPool->lock);
            }
        }

        //Get the next task from the queue
        RemoveFromHeadOfQueue(threadPool, &currentTask);

        //We're done modifying the queue, so release the lock
        pthread_mutex_unlock(&threadPool->lock);

        //Signal that the queue is not full (since we just removed a task, freeing at least one spot in the queue)
        pthread_cond_signal(&threadPool->not_full);

        //Execute the task
        currentTask.function(currentTask.argument);
    }

    //This is an undefined state (the thread exits in the while loop), but we don't want the compiler to complain
    return NULL;
}
