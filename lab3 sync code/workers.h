#ifndef WORKERS_H
#define WORKERS_H

#include "shared_data.h"

void producer(SharedData* sd, char type);
void consumer1(SharedData* sd);
void consumer2(SharedData* sd);

#endif
