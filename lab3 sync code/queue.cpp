#include "queue.h"
#include <cstring>

FifoQueue::FifoQueue(SharedData* data) : sd(data) {}

void FifoQueue::push(char type, uint8_t value) {
    sd->buf[sd->count].type = type;
    sd->buf[sd->count].value = value;
    sd->count++;
    if (type == 'A') sd->count_a++;
    else if (type == 'B') sd->count_b++;
    else sd->count_c++;
}

bool FifoQueue::find_and_remove(char type, Element* out) {
    for (int i = 0; i < sd->count; i++) {
        if (sd->buf[i].type == type) {
            *out = sd->buf[i];
            memmove(&sd->buf[i], &sd->buf[i + 1],
                    (sd->count - i - 1) * sizeof(Element));
            sd->count--;
            if (type == 'A') sd->count_a--;
            else if (type == 'B') sd->count_b--;
            else sd->count_c--;
            return true;
        }
    }
    return false;
}

int FifoQueue::first_ab_index() const {
    for (int i = 0; i < sd->count; i++)
        if (sd->buf[i].type == 'A' || sd->buf[i].type == 'B')
            return i;
    return -1;
}
