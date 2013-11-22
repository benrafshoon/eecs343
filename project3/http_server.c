#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "thread_pool.h"
#include "seats.h"
#include "util.h"
#include "pthread.h"

#define BUFSIZE 1024
#define FILENAMESIZE 100

#define NUM_THREADS 2
#define QUEUE_SIZE 4000

void shutdown_server(int);

int listenfd;
threadpool_t* threadpool;
/*
static void threadTestWork(void* argument) {
    int* number = (int*)argument;
    printf("\n\nTest task %i\n", *number);
    sleep(1);
    printf("Test task %i done\n\n\n", *number);
    free(argument);
}
*/
int main(int argc,char *argv[])
{
    /*
    threadpool_t* threadPool = threadpool_create(10, 3);

    sleep(1);

    int c;
    for(c = 0; c < 10; c++) {
        int* number = (int*)malloc(sizeof(int));
        *number = c;
        threadpool_add_task(threadPool, &threadTestWork, number);
    }

    sleep(15);
    threadpool_destroy(threadPool);
    return 0;
    */

    int flag, num_seats = 20;
    int connfd = 0;
    struct sockaddr_in serv_addr;

    char send_buffer[BUFSIZE];

    listenfd = 0;

    int server_port = 8080;

    if (argc > 1)
    {
        num_seats = atoi(argv[1]);
    }

    if (server_port < 1500)
    {
        fprintf(stderr,"INVALID PORT NUMBER: %d; can't be < 1500\n",server_port);
        exit(-1);
    }

    if (signal(SIGINT, shutdown_server) == SIG_ERR)
        printf("Issue registering SIGINT handler");

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( listenfd < 0 ){
        perror("Socket");
        exit(errno);
    }
    printf("Established Socket: %d\n", listenfd);
    flag = 1;
    setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag) );

    // initialize the threadpool
    // Set the number of threads and size of the queue
    // threadpool = threadpool_create(0,0);
/*
    //create an array of threads
    pthread_t threads[NUM_THREADS];
    int rc; //the particular thread
    int t=0; //index in the threads
    // Load the seats;

*/
    threadpool = threadpool_create(NUM_THREADS, QUEUE_SIZE);

    load_seats(num_seats); //TODO read from argv
    // set server address
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(send_buffer, '0', sizeof(send_buffer));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(server_port);

    // bind to socket
    if ( bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) != 0)
    {
        perror("socket--bind");
        exit(errno);
    }

    // listen for incoming requests
    listen(listenfd, 10);

    // handle connections loop (forever)
    while(1)
    {
        //printf("Accepting connection\n");
        //int* connectionFileDescriptor = (int*)malloc(sizeof(int));
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        //printf("Connection accepted\n");


        threadpool_add_task(threadpool, &handle_connection, connfd);
        //printf("Task added\n");
        //handle_connection(&connfd);
        /*rc = pthread_create(&threads[t], NULL, handle_connection, (void *) &connfd);
        if (rc){
         printf("ERROR; return code from pthread_create() is %d\n", rc);
         exit(-1);
        }
        t++;
        */
    }
}

void shutdown_server(int signo){
    threadpool_destroy(threadpool);
    unload_seats();
    close(listenfd);
    exit(0);
}
