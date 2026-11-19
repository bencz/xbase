/* bench/shared_counter.prg in C: a counter shared by 8 threads, contended: each thread adds 1 to an atomic integer
 * 250,000 times with an atomic add, then 50,000 times with a compare-and-exchange loop, then 50,000 times to an array
 * element under a mutex. No increment may be lost, whatever the scheduling. */
#include "bench.h"
#include <pthread.h>
#include <stdatomic.h>

static atomic_llong counter, cas_counter;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static int64_t box[1];

static void *work(void *arg) {
  (void)arg;
  for (int i = 1; i <= 250000; ++i) atomic_fetch_add(&counter, 1);
  for (int i = 1; i <= 50000; ++i)
    for (;;) {
      long long n = atomic_load(&cas_counter);
      if (atomic_compare_exchange_strong(&cas_counter, &n, n + 1)) break;
    }
  for (int i = 1; i <= 50000; ++i) {
    pthread_mutex_lock(&m);
    box[0]++;
    pthread_mutex_unlock(&m);
  }
  return NULL;
}

int main(void) {
  long long t = bench_ms();
  pthread_t th[8];
  for (int k = 0; k < 8; ++k) pthread_create(&th[k], NULL, work, NULL);
  for (int k = 0; k < 8; ++k) pthread_join(th[k], NULL);
  bench_report("shared_counter", t,
               atomic_load(&counter) == 8 * 250000 && atomic_load(&cas_counter) == 8 * 50000 && box[0] == 8 * 50000);
  return 0;
}
