/* bench/async_pi.prg in C: the Basel sum over 32 threads of 50,000,000 terms each, joined in order; pi is
 * Sqrt( 6 * sum ). */
#include "bench.h"
#include <math.h>
#include <pthread.h>
#include <stdint.h>

#define TASKS 32
#define TERMS 50000000LL

static double partial[TASKS];

static void *basel(void *arg) {
  long long i = (long long)(intptr_t)arg;
  double s = 0;
  for (long long n = i * TERMS + 1; n <= (i + 1) * TERMS; ++n) {
    double inv = 1.0 / (double)n;
    s += inv * inv;
  }
  partial[i] = s;
  return NULL;
}

int main(void) {
  long long t = bench_ms();
  pthread_t th[TASKS];
  for (int i = 0; i < TASKS; ++i) pthread_create(&th[i], NULL, basel, (void *)(intptr_t)i);
  double total = 0;
  for (int i = 0; i < TASKS; ++i) {
    pthread_join(th[i], NULL);
    total += partial[i];
  }
  double pi = sqrt(total * 6);
  bench_report("async_pi", t, fabs(3.141592653589793 - pi) < 0.000000001);
  return 0;
}
