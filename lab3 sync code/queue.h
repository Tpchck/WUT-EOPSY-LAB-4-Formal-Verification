#ifndef QUEUE_H
#define QUEUE_H

#include "shared_data.h"

class FifoQueue {
    SharedData* sd;
public:
    FifoQueue(SharedData* data);

    void push(char type, uint8_t value);
    bool find_and_remove(char type, Element* out);
    int first_ab_index() const;
};

#endif
