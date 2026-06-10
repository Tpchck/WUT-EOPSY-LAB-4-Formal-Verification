// =========================================================================
// Lab 3 — Producer-Consumer Synchronization (POSIX Semaphores)
// Combined source for formal verification submission
// =========================================================================
// System: 3 producers (A, B, C), 2 consumers (C1, C2)
// Shared bounded FIFO queue, POSIX semaphores for synchronization
// =========================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

// ---------- Configuration ----------
#define QUEUE_CAPACITY 16
#define PROD_MIN       64
#define PROD_MAX       1128
#define SHM_NAME       "/sync_lab3_335954"

// ---------- Data structures ----------
struct Element {
    char type;       // 'A', 'B', or 'C'
    uint8_t value;
};

struct SharedData {
    Element buf[QUEUE_CAPACITY];
    int count;
    int count_a, count_b, count_c;

    sem_t mutex;     // binary — critical section
    sem_t slots;     // counting — limits queue size
    sem_t wake_c1;   // binary — wake consumer 1
    sem_t wake_c2;   // binary — wake consumer 2

    int prod_done;
    int total_produced[3];
    int all_done;
    int no_sync;
    int use_delay;
    int verbose;
    int fixed_count[3];
};

// ---------- Queue operations ----------
static void queue_push(SharedData* sd, char type, uint8_t value) {
    sd->buf[sd->count].type = type;
    sd->buf[sd->count].value = value;
    sd->count++;
    if (type == 'A') sd->count_a++;
    else if (type == 'B') sd->count_b++;
    else sd->count_c++;
}

static bool queue_find_and_remove(SharedData* sd, char type, Element* out) {
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

static int queue_first_ab_index(SharedData* sd) {
    for (int i = 0; i < sd->count; i++)
        if (sd->buf[i].type == 'A' || sd->buf[i].type == 'B')
            return i;
    return -1;
}

// ---------- Semaphore helpers ----------
static void sw(sem_t *s, int skip) {
    if (!skip) sem_wait(s);
}

static void sp(sem_t *s, int skip) {
    if (!skip) sem_post(s);
}

// ---------- Dispatch logic ----------
// Wakes the appropriate consumer based on queue state
static bool c2_can_act(SharedData *sd) {
    if (sd->count_c == 0) return false;
    if (sd->count_a > 0 && sd->count_b > 0) return true;
    if (sd->count_a == 0 && sd->count_b == 0) return true;
    if (sd->count >= QUEUE_CAPACITY) return true;
    return false;
}

static void dispatch(SharedData *sd) {
    if (sd->count == 0 && sd->prod_done >= 3) {
        sd->all_done = 1;
        sem_post(&sd->wake_c1);
        sem_post(&sd->wake_c2);
        return;
    }
    if (sd->no_sync) return;

    if ((sd->count_a > 0 || sd->count_b > 0) && sd->count_c == 0)
        sem_post(&sd->wake_c1);

    if (c2_can_act(sd))
        sem_post(&sd->wake_c2);
}

// ---------- Producer ----------
void producer(SharedData *sd, char type) {
    int idx = (type == 'A') ? 0 : (type == 'B') ? 1 : 2;

    srand((unsigned)getpid());
    int n = sd->fixed_count[idx] > 0
                ? sd->fixed_count[idx]
                : PROD_MIN + rand() % (PROD_MAX - PROD_MIN + 1);

    sw(&sd->mutex, sd->no_sync);
    sd->total_produced[idx] = n;
    sp(&sd->mutex, sd->no_sync);

    printf("[P-%c] generating %d elements\n", type, n);

    for (int i = 0; i < n; i++) {
        if (sd->use_delay) usleep(200 + rand() % 800);

        sw(&sd->slots, sd->no_sync);   // wait for free slot
        sw(&sd->mutex, sd->no_sync);   // enter critical section

        queue_push(sd, type, rand() % 256);
        dispatch(sd);

        sp(&sd->mutex, sd->no_sync);   // release mutex
    }

    sw(&sd->mutex, sd->no_sync);
    sd->prod_done++;
    dispatch(sd);
    sp(&sd->mutex, sd->no_sync);

    printf("[P-%c] done (%d elements)\n", type, n);
}

// ---------- Consumer 1 ----------
// Consumes single A or B when no C is present
void consumer1(SharedData *sd) {
    int ca = 0, cb = 0;

    while (1) {
        if (sd->no_sync) usleep(600);
        else sem_wait(&sd->wake_c1);     // wait for wake signal

        sw(&sd->mutex, sd->no_sync);     // enter critical section

        if (sd->all_done) { sp(&sd->mutex, sd->no_sync); break; }

        if (sd->count_c == 0 && (sd->count_a > 0 || sd->count_b > 0)) {
            int fi = queue_first_ab_index(sd);
            char target = sd->buf[fi].type;
            Element e;
            queue_find_and_remove(sd, target, &e);

            if (e.type == 'A') ca++; else cb++;

            sp(&sd->slots, sd->no_sync); // release slot
            dispatch(sd);

            if (sd->verbose)
                printf("[C-1] %c val=%3d | A:%d B:%d C:%d total:%d\n",
                       e.type, e.value, sd->count_a, sd->count_b, sd->count_c, sd->count);
        }

        if (sd->all_done || (sd->count == 0 && sd->prod_done >= 3)) {
            sd->all_done = 1;
            sem_post(&sd->wake_c2);
            sp(&sd->mutex, sd->no_sync);
            break;
        }

        sp(&sd->mutex, sd->no_sync);
    }

    printf("[C-1] done (A:%d B:%d total:%d)\n", ca, cb, ca + cb);
}

// ---------- Consumer 2 ----------
// Consumes triple (A+B+C) or single C
void consumer2(SharedData *sd) {
    int triples = 0, singles = 0;

    while (1) {
        if (sd->no_sync) usleep(600);
        else sem_wait(&sd->wake_c2);     // wait for wake signal

        sw(&sd->mutex, sd->no_sync);     // enter critical section

        if (sd->all_done) { sp(&sd->mutex, sd->no_sync); break; }

        if (sd->count_c > 0 && sd->count_a > 0 && sd->count_b > 0) {
            // Pop triple (A+B+C)
            Element ea, eb, ec;
            queue_find_and_remove(sd, 'A', &ea);
            queue_find_and_remove(sd, 'B', &eb);
            queue_find_and_remove(sd, 'C', &ec);
            triples++;

            sp(&sd->slots, sd->no_sync);
            sp(&sd->slots, sd->no_sync);
            sp(&sd->slots, sd->no_sync); // release 3 slots
            dispatch(sd);

            if (sd->verbose)
                printf("[C-2] {A=%3d,B=%3d,C=%3d} | A:%d B:%d C:%d total:%d\n",
                       ea.value, eb.value, ec.value,
                       sd->count_a, sd->count_b, sd->count_c, sd->count);
        } else if (sd->count_c > 0 && ((sd->count_a == 0 && sd->count_b == 0) ||
                                        sd->count >= QUEUE_CAPACITY)) {
            // Pop single C
            Element ec;
            queue_find_and_remove(sd, 'C', &ec);
            singles++;

            sp(&sd->slots, sd->no_sync); // release 1 slot
            dispatch(sd);

            if (sd->verbose)
                printf("[C-2] C val=%3d | A:%d B:%d C:%d total:%d\n",
                       ec.value, sd->count_a, sd->count_b, sd->count_c, sd->count);
        }

        if (sd->all_done || (sd->count == 0 && sd->prod_done >= 3)) {
            sd->all_done = 1;
            sem_post(&sd->wake_c1);
            sp(&sd->mutex, sd->no_sync);
            break;
        }

        sp(&sd->mutex, sd->no_sync);
    }

    printf("[C-2] done (triples:%d singles:%d total:%d)\n",
           triples, singles, triples * 3 + singles);
}

// ---------- Main ----------
int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);
    int no_sync = 0, use_delay = 0, verbose = 0;
    int counts[3] = {0, 0, 0};
    int ci = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-sync") == 0) no_sync = 1;
        else if (strcmp(argv[i], "--delay") == 0) use_delay = 1;
        else if (strcmp(argv[i], "--verbose") == 0) verbose = 1;
        else if (ci < 3) counts[ci++] = atoi(argv[i]);
    }

    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (ftruncate(fd, sizeof(SharedData)) == -1) return 1;
    SharedData* sd = (SharedData*)mmap(NULL, sizeof(SharedData),
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED, fd, 0);
    close(fd);

    memset(sd, 0, sizeof(SharedData));
    sd->no_sync = no_sync;
    sd->use_delay = use_delay;
    sd->verbose = verbose;
    sd->fixed_count[0] = counts[0];
    sd->fixed_count[1] = counts[1];
    sd->fixed_count[2] = counts[2];

    sem_init(&sd->mutex, 1, 1);
    sem_init(&sd->slots, 1, QUEUE_CAPACITY);
    sem_init(&sd->wake_c1, 1, 0);
    sem_init(&sd->wake_c2, 1, 0);

    printf("=== Sync Lab 3 | SYNC=%s DELAY=%s ===\n\n",
           no_sync ? "OFF" : "ON", use_delay ? "ON" : "OFF");

    pid_t pids[5];
    const char types[] = {'A', 'B', 'C'};

    for (int i = 0; i < 3; i++) {
        pids[i] = fork();
        if (pids[i] == 0) { producer(sd, types[i]); _exit(0); }
    }

    pids[3] = fork();
    if (pids[3] == 0) { consumer1(sd); _exit(0); }

    pids[4] = fork();
    if (pids[4] == 0) { consumer2(sd); _exit(0); }

    for (int i = 0; i < 5; i++)
        waitpid(pids[i], NULL, 0);

    printf("\n=== Results ===\n");
    printf("Produced: A=%d B=%d C=%d total=%d\n",
           sd->total_produced[0], sd->total_produced[1], sd->total_produced[2],
           sd->total_produced[0] + sd->total_produced[1] + sd->total_produced[2]);

    sem_destroy(&sd->mutex);
    sem_destroy(&sd->slots);
    sem_destroy(&sd->wake_c1);
    sem_destroy(&sd->wake_c2);
    munmap(sd, sizeof(SharedData));
    shm_unlink(SHM_NAME);

    return 0;
}
