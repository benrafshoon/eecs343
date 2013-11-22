#ifndef _SEAT_OPERATIONS_H_
#define _SEAT_OPERATIONS_H_

typedef enum
{
    AVAILABLE,
    PENDING,
    OCCUPIED
} seat_state_t;

typedef struct seat_struct
{
    int id;
    int customer_id;
    seat_state_t state;
    struct seat_struct* next;
} seat_t;


void load_seats(int);
void unload_seats();

void list_seats(char* buf, int bufsize);
void view_seat(char* buf, int bufsize, int seat_num, int customer_num, int customer_priority, pthread_mutex_t *seat_locks);
void confirm_seat(char* buf, int bufsize, int seat_num, int customer_num, int customer_priority, pthread_mutex_t *seat_locks);
void cancel(char* buf, int bufsize, int seat_num, int customer_num, int customer_priority, pthread_mutex_t *seat_locks);
int seat_locks_destroy(pthread_mutex_t *seat_locks);

#endif
