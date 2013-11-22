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

#include <time.h>

#include "seats.h"
#include "file_cache.h"

#define BUFSIZE 1024


int writenbytes(int,char *,int);
int readnbytes(int,char *,int);
int get_line(int, char*,int);

int parse_int_arg(char* filename, char* arg);

void handle_connection(int connfd)
{
    long initialTime;
    long finalTime;
    struct timespec time;

    clock_gettime(CLOCK_REALTIME, &time);
    initialTime = time.tv_nsec;

    int fd;
    char buf[BUFSIZE+1];
    char instr[20];
    char file[100];
    char type[20];

    int i=0;
    int j=0;

    char *ok_response = "HTTP/1.0 200 OK\r\n"\
                           "Content-type: text/html\r\n\r\n";

    char *notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"\
                            "Content-type: text/html\r\n\r\n"\
                            "<html><body bgColor=white text=black>\n"\
                            "<h2>404 FILE NOT FOUND</h2>\n"\
                            "</body></html>\n";

    char *bad_request = "HTTP/1.0 400 BAD REQUEST\r\n"\
                              "Content-type: text/html\r\n\r\n"\
                              "<html><body><h2>BAD REQUEST</h2>"\
                              "</body></html>\n";


    // first read loop -- get request and headers

    // parse request to get file name
    // Assumption: this is a GET request and filename contains no spaces
    // parse request
    // get headers

    //Expection Format: 'GET filenane.txt HTTP/1.X'

    get_line(connfd, buf, BUFSIZE);
    //printf("line:%s\n", buf);
    //parse out instruction
    while( !isspace(buf[j]) && (i < sizeof(instr) - 1))
    {
        instr[i] = buf[i];
        i++;
        j++;
    }
    j+=2;
    instr[i] = '\0';


    //Only accept GET requests
    if (strncmp(instr, "GET", 3) != 0) {
        writenbytes(connfd, bad_request, strlen(bad_request));
        close(connfd);
        return;
    }

    //parse out filename
    i=0;
    while (!isspace(buf[j]) && (i < sizeof(file) - 1))
    {
        file[i] = buf[j];
        i++;
        j++;
    }
    j++;
    file[i] = '\0';

    //parse out type
    i=0;
    while (!isspace(buf[j]) && (buf[j] != '\0') && (i < sizeof(type) - 1))
    {
        type[i] = buf[j];
        i++;
        j++;
    }
    type[i] = '\0';

    while (get_line(connfd, buf, BUFSIZE) > 0)
    {
        //ignore headers -> (for now)
    }

    char* resource = strtok(file, "?");
    char* arg1Name = strtok(NULL, "=");
    char* arg1Value = strtok(NULL, "&?");
    char* arg2Name = strtok(NULL, "=");
    char* arg2Value = strtok(NULL, "&?");

    int seat_id = 0;
    int user_id = 0;
    int customer_priority = 0;

    if(arg1Name != NULL) {
        if(strcmp(arg1Name, "seat") == 0) {
            seat_id = atoi(arg1Value);
        } else if(strcmp(arg1Name, "user") == 0) {
            user_id = atoi(arg1Value);
        }
    }
    if(arg2Name != NULL) {
        if(strcmp(arg2Name, "seat") == 0) {
            seat_id = atoi(arg2Value);
        } else if(strcmp(arg2Name, "user") == 0) {
            user_id = atoi(arg2Value);
        }
    }
    //printf("%s seat %i, user %i\n", resource, seat_id, user_id);


/*
    int length;
    for(i = 0; i < strlen(file); i++)
    {
        if(file[i] == '?')
            break;
    }
    length = i;

    char resource[length+1];

    if (length > strlen(file)) {
      length = strlen(file);
    }

    strncpy(resource, file, length);
    resource[length] = 0;


*/

    // Check if the request is for one of our operations
    if (strcmp(resource, "list_seats") == 0)
    {
        list_seats(buf, BUFSIZE);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
    }
    else if(strcmp(resource, "view_seat") == 0)
    {
        view_seat(buf, BUFSIZE, seat_id, user_id, customer_priority);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
    }
    else if(strcmp(resource, "confirm") == 0)
    {
        confirm_seat(buf, BUFSIZE, seat_id, user_id, customer_priority);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
    }
    else if(strcmp(resource, "cancel") == 0)
    {
        cancel(buf, BUFSIZE, seat_id, user_id, customer_priority);
        // send headers
        writenbytes(connfd, ok_response, strlen(ok_response));
        // send data
        writenbytes(connfd, buf, strlen(buf));
    }
    else
    {
        // try to open the file
        if ((fd = open(resource, O_RDONLY)) == -1)
        {
            writenbytes(connfd, notok_response, strlen(notok_response));
        }
        else
        {
            FileCache* cacheEntry = GetCacheEntry(resource);

            if(cacheEntry != NULL) {
                writenbytes(connfd, ok_response, strlen(ok_response));
                writenbytes(connfd, cacheEntry->buffer, cacheEntry->size);
            } else {

                // send headers
                writenbytes(connfd, ok_response, strlen(ok_response));
                // send file
                int ret;
                while ( (ret = read(fd, buf, BUFSIZE)) > 0) {
                    writenbytes(connfd, buf, ret);
                }
            }


            // close file and free space
            close(fd);
        }
    }

    close(connfd);

    clock_gettime(CLOCK_REALTIME, &time);
    finalTime = time.tv_nsec;
    //printf("Request %s time %li us\n", file, (finalTime - initialTime)/100);
}

int get_line(int fd, char *buf, int size)
{

    int i=0;
    char c = '\0';
    int n;

    while((i < size-1) && (c != '\n'))
    {
        n = readnbytes(fd, &c, 1);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = readnbytes(fd, &c, 1);

                if ((n > 0) && (c == '\n'))
                {
                    //this is an \r\n endline for request
                    //we want to then return the line
                    //readnbytes(fd, &c, 1);
                    continue;
                }
                else
                {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }
        else
        {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

int readnbytes(int fd,char *buf,int size)
{
    int rc = 0;
    int totalread = 0;
    while ((rc = read(fd,buf+totalread,size-totalread)) > 0) {
        totalread += rc;
    }


    if (rc < 0)
    {
        return -1;
    }
    else
        return totalread;
}

int writenbytes(int fd,char *str,int size)
{
    int rc = 0;
    int totalwritten =0;
    while ((rc = write(fd,str+totalwritten,size-totalwritten)) > 0)
        totalwritten += rc;

    if (rc < 0)
        return -1;
    else
        return totalwritten;
}

int parse_int_arg(char* filename, char* arg)
{
    int i;
    bool found_value_start = false;
    bool found_arg_list_start = false;
    int seatnum = 0;
    for(i=0; i < strlen(filename); i++)
    {
        if (!found_arg_list_start)
        {
            if (filename[i] == '?')
            {
                found_arg_list_start = true;
            }
            continue;
        }
        if (!found_value_start && strncmp(&filename[i], arg, strlen(arg)) == 0)
        {
            found_value_start = true;
            i += strlen(arg);
        }
        if (found_value_start)
        {
            if(isdigit(filename[i]))
            {
                seatnum = seatnum * 10 + (int) filename[i] - (int) '0';
                continue;
            }
            else
            {
                break;
            }
        }
    }
    return seatnum;
}
