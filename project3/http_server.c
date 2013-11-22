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
#define NUM_THREADS 50

void shutdown_server(int);

int listenfd;
threadpool_t* threadpool;
pthread_mutex_t seat_locks[20];

/*static void threadTestWork(void* argument) {
    int* number = (int*)argument;
    printf("\n\nTest task %i\n", *number);
    sleep(1);
    printf("Test task %i done\n\n\n", *number);
    free(argument);
}*/

int main(int argc,char *argv[])
{
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

    threadpool = threadpool_create(NUM_THREADS,NUM_THREADS);

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
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
        threadpool_add_task(threadpool, handle_connection, (void* ) &connfd);
    }
    pthread_exit(NULL);
}

void shutdown_server(int signo){
    threadpool_destroy(threadpool);
    seat_locks_destroy(seat_locks);
    unload_seats();
    close(listenfd);
    exit(0);
}
