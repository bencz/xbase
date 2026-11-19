/* bench/producer_consumer.prg in C: producers and consumers on threads over a bounded queue of 64 (an array, taken
 * from the front by moving the others down) guarded by a mutex and two condition variables (not full, not empty):
 * 4 producers put 50,000 numbers each, 4 consumers take them until each finds the end mark the program puts once the
 * producers are joined. The sum and count of what was taken must be what was put, whatever the scheduling. */
#include "bench.h"
#include <pthread.h>
#include <string.h>

#define ITEMS 50000
#define END_MARK INT64_MIN /* NIL */

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER, not_empty = PTHREAD_COND_INITIALIZER;
static int64_t queue[64];
static int len;

static void put(int64_t x) {
  pthread_mutex_lock(&m);
  while (len >= 64) pthread_cond_wait(&not_full, &m);
  queue[len++] = x;
  pthread_cond_signal(&not_empty);
  pthread_mutex_unlock(&m);
}

/* Producer p puts p * 1000000 + i for i from 1 to ITEMS. */
static void *produce(void *arg) {
  int64_t p = (int64_t)(intptr_t)arg;
  for (int64_t i = 1; i <= ITEMS; ++i) put(p * 1000000 + i);
  return NULL;
}

typedef struct {
  int64_t sum, count;
} Taken;

static void *consume(void *arg) {
  Taken *r = arg;
  for (;;) {
    pthread_mutex_lock(&m);
    while (len == 0) pthread_cond_wait(&not_empty, &m);
    int64_t x = queue[0];
    memmove(queue, queue + 1, (size_t)(--len) * sizeof *queue);
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&m);
    if (x == END_MARK) break;
    r->sum += x;
    ++r->count;
  }
  return NULL;
}

int main(void) {
  long long t = bench_ms();
  pthread_t producers[4], consumers[4];
  Taken taken[4] = {{0, 0}};
  int64_t sum = 0, count = 0;
  for (int k = 0; k < 4; ++k) pthread_create(&consumers[k], NULL, consume, &taken[k]);
  for (int k = 0; k < 4; ++k) pthread_create(&producers[k], NULL, produce, (void *)(intptr_t)(k + 1));
  for (int k = 0; k < 4; ++k) pthread_join(producers[k], NULL);
  for (int k = 0; k < 4; ++k) put(END_MARK);
  for (int k = 0; k < 4; ++k) {
    pthread_join(consumers[k], NULL);
    sum += taken[k].sum;
    count += taken[k].count;
  }
  bench_report("producer_consumer", t,
               count == 4 * ITEMS && sum == 10000000LL * ITEMS + 2LL * ITEMS * (ITEMS + 1));
  return 0;
}
