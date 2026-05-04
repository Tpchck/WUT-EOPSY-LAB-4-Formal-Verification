#include "shared_data.h"
#include "workers.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define SHM_NAME "/sync_lab3_335954"

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
        if (pids[i] == 0) {
            producer(sd, types[i]);
            _exit(0);
        }
    }

    pids[3] = fork();
    if (pids[3] == 0) {
        consumer1(sd);
        _exit(0);
    }

    pids[4] = fork();
    if (pids[4] == 0) {
        consumer2(sd);
        _exit(0);
    }

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
