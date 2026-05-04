#include "workers.h"
#include "queue.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

static void sw(sem_t *s, int skip) {
  if (!skip) // no-op when sync disabled
    sem_wait(s);
}

static void sp(sem_t *s, int skip) {
  if (!skip) // no-op when sync disabled
    sem_post(s);
}

static bool c2_can_act(SharedData *sd) {
  if (sd->count_c == 0)
    return false; // no C elements available
  if (sd->count_a > 0 && sd->count_b > 0)
    return true;
  if (sd->count_a == 0 && sd->count_b == 0)
    return true;
  if (sd->count >= QUEUE_CAPACITY)
    return true;
  return false;
}

static void dispatch(SharedData *sd) {
  if (sd->count == 0 && sd->prod_done >= 3) {
    sd->all_done = 1;
    sem_post(&sd->wake_c1);
    sem_post(&sd->wake_c2);
    return;
  }
  if (sd->no_sync)
    return;

  if ((sd->count_a > 0 || sd->count_b > 0) && sd->count_c == 0)
    sem_post(&sd->wake_c1);

  if (c2_can_act(sd))
    sem_post(&sd->wake_c2);
}

void producer(SharedData *sd, char type) {
  FifoQueue q(sd);
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
    if (sd->use_delay)
      usleep(200 + rand() % 800);

    sw(&sd->slots, sd->no_sync);
    sw(&sd->mutex, sd->no_sync);

    q.push(type, rand() % 256);
    dispatch(sd);

    sp(&sd->mutex, sd->no_sync);
  }

  sw(&sd->mutex, sd->no_sync);
  sd->prod_done++;
  dispatch(sd);
  sp(&sd->mutex, sd->no_sync);

  printf("[P-%c] done (%d elements)\n", type, n);
}

void consumer1(SharedData *sd) {
  FifoQueue q(sd);
  int ca = 0, cb = 0; // consumed counts

  while (1) {
    if (sd->no_sync)
      usleep(600);
    else
      sem_wait(&sd->wake_c1);

    sw(&sd->mutex, sd->no_sync);

    if (sd->all_done) {
      sp(&sd->mutex, sd->no_sync);
      break;
    }

    if (sd->count_c == 0 && (sd->count_a > 0 || sd->count_b > 0)) {
      int fi = q.first_ab_index();
      char target = sd->buf[fi].type;
      Element e;
      q.find_and_remove(target, &e);

      if (e.type == 'A')
        ca++;
      else
        cb++;

      sp(&sd->slots, sd->no_sync);
      dispatch(sd);

      if (sd->verbose)
        printf("[C-1] %c val=%3d | A:%d B:%d C:%d total:%d\n", e.type, e.value,
               sd->count_a, sd->count_b, sd->count_c, sd->count);
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

void consumer2(SharedData *sd) {
  FifoQueue q(sd);
  int triples = 0, singles = 0; // consumer2 stats

  while (1) {
    if (sd->no_sync)
      usleep(600);
    else
      sem_wait(&sd->wake_c2);

    sw(&sd->mutex, sd->no_sync);

    if (sd->all_done) {
      sp(&sd->mutex, sd->no_sync);
      break;
    }

    if (sd->count_c > 0 && sd->count_a > 0 && sd->count_b > 0) {
      Element ea, eb, ec;
      q.find_and_remove('A', &ea);
      q.find_and_remove('B', &eb);
      q.find_and_remove('C', &ec);
      triples++;

      sp(&sd->slots, sd->no_sync);
      sp(&sd->slots, sd->no_sync);
      sp(&sd->slots, sd->no_sync);
      dispatch(sd);

      if (sd->verbose)
        printf("[C-2] {A=%3d,B=%3d,C=%3d} | A:%d B:%d C:%d total:%d\n",
               ea.value, eb.value, ec.value, sd->count_a, sd->count_b,
               sd->count_c, sd->count);
    } else if (sd->count_c > 0 && ((sd->count_a == 0 && sd->count_b == 0) ||
                                   sd->count >= QUEUE_CAPACITY)) {
      Element ec;
      q.find_and_remove('C', &ec);
      singles++;

      sp(&sd->slots, sd->no_sync);
      dispatch(sd);

      if (sd->verbose)
        printf("[C-2] C val=%3d | A:%d B:%d C:%d total:%d\n", ec.value,
               sd->count_a, sd->count_b, sd->count_c, sd->count);
    }

    if (sd->all_done || (sd->count == 0 && sd->prod_done >= 3)) {
      sd->all_done = 1;
      sem_post(&sd->wake_c1);
      sp(&sd->mutex, sd->no_sync);
      break;
    }

    sp(&sd->mutex, sd->no_sync);
  }

  printf("[C-2] done (triples:%d singles:%d total:%d)\n", triples, singles,
         triples * 3 + singles);
}
