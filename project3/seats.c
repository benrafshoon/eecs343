#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "seats.h"

//The seat list is a fixed size array
seat_t* seat_list = NULL;
int number_of_seats;

char seat_state_to_char(seat_state_t);


//The customer_id and state of each seat are synchronized using a solution to the readers-writer problem
//Multiple people can read, only one can write
//Writer must wait until there are no readers before writinh
//The position in the array is the seat_id.  The structure of the array is fixed, so it doesn't need any sort of synchronization

//Readers call StartRead and EndRead to indicate the beginning and end, repectively, of a section of code
//that reads the state or customer_id of a seat
static void StartRead(seat_t* seat);
static void EndRead(seat_t* seat);

// Writers call StartWrite and EndWrite to indicate the beginning and end, respectively, or a section of code
//that writes to the state or customer_id of a seat
static void StartWrite(seat_t* seat);
static void EndWrite(seat_t* seat);

void list_seats(char* buf, int bufsize)
{
    seat_t* curr;
    int index = 0;
    int seat_id = 0;
    for(curr = seat_list; curr < seat_list + number_of_seats && index < bufsize + strlen("%d %c,"); curr++) {
        //Mark that we are reading when we get the seat state
        StartRead(curr);
        int length = snprintf(buf+index, bufsize-index,
                "%d %c,", seat_id, seat_state_to_char(curr->state));
        EndRead(curr);

        if (length > 0)
            index = index + length;
        seat_id++;
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
        //Mark that we are writing since we will update the seat state and customer id
        StartWrite(curr);
        if(curr->state == AVAILABLE || (curr->state == PENDING && curr->customer_id == customer_id))

        {
            snprintf(buf, bufsize, "Confirm seat: %d %c ?\n\n",
                    seat_id, seat_state_to_char(curr->state));
            curr->state = PENDING;
            curr->customer_id = customer_id;
        }
        else
        {
            snprintf(buf, bufsize, "Seat unavailable\n\n");
        }
        EndWrite(curr);

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

        //Mark that we are writing because we will change the seat state
        StartWrite(curr);
        if(curr->state == PENDING && curr->customer_id == customer_id )
        {
            snprintf(buf, bufsize, "Seat confirmed: %d %c\n\n",
                    seat_id, seat_state_to_char(curr->state));
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
        //Mark that we are writing since we are changing the seat state
        StartWrite(curr);
        if(curr->state == PENDING && curr->customer_id == customer_id )
        {
            snprintf(buf, bufsize, "Seat request cancelled: %d %c\n\n",
                    seat_id, seat_state_to_char(curr->state));
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

        return;

    } else {
        snprintf(buf, bufsize, "Seat not found\n\n");
        return;
    }
}

//Initialize the array of seats
void load_seats(int number_of_seats_to_load)
{
    number_of_seats = number_of_seats_to_load;

    seat_list = malloc(sizeof(seat_t) * number_of_seats);

    int i;
    for(i = 0; i < number_of_seats; i++)
    {
        seat_t* current_seat = &seat_list[i];
        current_seat->customer_id = -1;
        current_seat->state = AVAILABLE;

        //Initialize the write semaphore to 1, the mutex lock, and the number of readers to 0
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
    pthread_mutex_lock(&seat->num_readers_lock);
    seat->num_readers++;
    if(seat->num_readers == 1) {
        //If we are the first reader, wait until the writer is done and prevent more writing
        sem_wait(&seat->writer_lock);
    }
    pthread_mutex_unlock(&seat->num_readers_lock);
}

static void EndRead(seat_t* seat) {
    pthread_mutex_lock(&seat->num_readers_lock);
    seat->num_readers--;
    if(seat->num_readers == 0) {
        //If we finished the last read, signal the writer to begin writing
        sem_post(&seat->writer_lock);
    }
    pthread_mutex_unlock(&seat->num_readers_lock);
}

static void StartWrite(seat_t* seat) {
    //Wait for all readers to finish reading and the current writer to finish writing
    sem_wait(&seat->writer_lock);
}

static void EndWrite(seat_t* seat) {
    sem_post(&seat->writer_lock);
}
