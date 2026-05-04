#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "element.h"
#include <semaphore.h>

struct SharedData {
    Element buf[QUEUE_CAPACITY];
    int count;
    int count_a, count_b, count_c;

    sem_t mutex;
    sem_t slots;
    sem_t wake_c1;
    sem_t wake_c2;

    int prod_done;
    int total_produced[3];
    int all_done;
    int no_sync; // 1 = semaphores disabled
    int use_delay; // 1 = artificial delays enabled
    int verbose; // 1 = detailed logging
    int fixed_count[3];
};

#endif
