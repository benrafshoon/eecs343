#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "seats.h"

seat_t* seat_list = NULL;
int number_of_seats;

char seat_state_to_char(seat_state_t);

static void StartRead(seat_t* seat);
static void EndRead(seat_t* seat);
static void StartWrite(seat_t* seat);
static void EndWrite(seat_t* seat);

void list_seats(char* buf, int bufsize)
{
    seat_t* curr;
    int index = 0;
    for(curr = seat_list; curr < seat_list + number_of_seats && index < bufsize + strlen("%d %c,"); curr++) {
        //CRITIAL SECTION
        StartRead(curr);
        int length = snprintf(buf+index, bufsize-index,
                "%d %c,", curr->id, seat_state_to_char(curr->state));
        EndRead(curr);
        //END CRITICAL SECTION
        if (length > 0)
            index = index + length;
    }

    if (index > 0)
        snprintf(buf+index-1, bufsize-index-1, "\n");
    else
        snprintf(buf, bufsize, "No seats not found\n\n");
}

void view_seat(char* buf, int bufsize,  int seat_id, int customer_id, int customer_priority)
{
    if(seat_id < number_of_seats) {
        seat_t* curr = &seat_list[seat_id];
        //CRITICAL SECTION
        StartWrite(curr);
        if(curr->state == AVAILABLE || (curr->state == PENDING && curr->customer_id == customer_id))

        {
            snprintf(buf, bufsize, "Confirm seat: %d %c ?\n\n",
                    curr->id, seat_state_to_char(curr->state));
            curr->state = PENDING;
            curr->customer_id = customer_id;
        }
        else
        {
            snprintf(buf, bufsize, "Seat unavailable\n\n");
        }
        EndWrite(curr);
        //END CRITICAL SECTION

        return;
    } else {
        snprintf(buf, bufsize, "Requested seat not found\n\n");
        return;
    }



}

void confirm_seat(char* buf, int bufsize, int seat_id, int customer_id, int customer_priority)
{

    if(seat_id < number_of_seats) {
         seat_t* curr = &seat_list[seat_id];
        //CRITICAL SECTION
        StartWrite(curr);
        if(curr->state == PENDING && curr->customer_id == customer_id )
        {
            snprintf(buf, bufsize, "Seat confirmed: %d %c\n\n",
                    curr->id, seat_state_to_char(curr->state));
            curr->state = OCCUPIED;
        }
        else if(curr->customer_id != customer_id )
        {
            snprintf(buf, bufsize, "Permission denied - seat held by another user\n\n");
        }
        else if(curr->state != PENDING)
        {
            snprintf(buf, bufsize, "No pending request\n\n");
        }
        EndWrite(curr);
        //END CRITICAL SECTION
        return;
    } else {
        snprintf(buf, bufsize, "Requested seat not found\n\n");
        return;

    }

}

void cancel(char* buf, int bufsize, int seat_id, int customer_id, int customer_priority)
{
    if(seat_id < number_of_seats) {
        seat_t* curr = &seat_list[seat_id];
        //CRITICAL SECTION
        StartWrite(curr);
        if(curr->state == PENDING && curr->customer_id == customer_id )
        {
            snprintf(buf, bufsize, "Seat request cancelled: %d %c\n\n",
                    curr->id, seat_state_to_char(curr->state));
            curr->state = AVAILABLE;
        }
        else if(curr->customer_id != customer_id )
        {
            snprintf(buf, bufsize, "Permission denied - seat held by another user\n\n");
        }
        else if(curr->state != PENDING)
        {
            snprintf(buf, bufsize, "No pending request\n\n");
        }
        EndWrite(curr);
        //END CRITICAL SECTION

        return;

    } else {
        snprintf(buf, bufsize, "Seat not found\n\n");
        return;
    }





}

void load_seats(int number_of_seats_to_load)
{
    number_of_seats = number_of_seats_to_load;

    seat_list = malloc(sizeof(seat_t) * number_of_seats);

    int i;
    for(i = 0; i < number_of_seats; i++)
    {
        seat_t* current_seat = &seat_list[i];
        current_seat->id = i;
        current_seat->customer_id = -1;
        current_seat->state = AVAILABLE;

        pthread_mutexattr_t mutexAttributes;
        pthread_mutexattr_init(&mutexAttributes);
        pthread_mutex_init(&current_seat->num_readers_lock, &mutexAttributes);

        current_seat->num_readers = 0;

        sem_init(&current_seat->writer_lock, 0, 1);
    }
}

void unload_seats()
{
    int i;
    for(i = 0; i < number_of_seats; i++) {
        seat_t* current_seat = &seat_list[i];
        pthread_mutex_destroy(&current_seat->num_readers_lock);
        sem_destroy(&current_seat->writer_lock);
    }
    free(seat_list);
}

char seat_state_to_char(seat_state_t state)
{
    switch(state)
    {
        case AVAILABLE:
            return 'A';
        case PENDING:
            return 'P';
        case OCCUPIED:
            return 'O';
    }

    return '0';
}

static void StartRead(seat_t* seat) {
    //printf("Requesting read lock on seat %i\n", seat->id);
    pthread_mutex_lock(&seat->num_readers_lock);
    seat->num_readers++;
    //printf("Now %i readers on seat %i\n", seat->num_readers, seat->id);
    if(seat->num_readers == 1) {
        //printf("Waiting for writer to finish on seat %i\n", seat->id);
        //If we are the only reader, wait until the writer is done
        sem_wait(&seat->writer_lock);
    }
    pthread_mutex_unlock(&seat->num_readers_lock);
    //printf("Reading on seat %i\n", seat->id);
}

static void EndRead(seat_t* seat) {
    //printf("Ending read on seat %i\n", seat->id);
    pthread_mutex_lock(&seat->num_readers_lock);
    seat->num_readers--;
    //printf("Now %i readers on seat %i\n", seat->num_readers, seat->id);
    if(seat->num_readers == 0) {
        //If we finished the last read, signal the writer to begin writing
        //printf("Signaling writer to write on seat %i\n", seat->id);
        sem_post(&seat->writer_lock);
    }
    pthread_mutex_unlock(&seat->num_readers_lock);
    //printf("Done reading on seat %i\n", seat->id);
}

static void StartWrite(seat_t* seat) {
    //printf("Requesting write on seat %i\n", seat->id);
    sem_wait(&seat->writer_lock);
    //printf("Writing on seat %i\n", seat->id);
}

static void EndWrite(seat_t* seat) {
    //printf("Finishing write on seat %i\n", seat->id);
    sem_post(&seat->writer_lock);
    //printf("Write finished on seat %i\n", seat->id);
}
