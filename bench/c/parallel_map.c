/* bench/parallel_map.prg in C: 1,000,000 numbers (1 to 1,000,000, from a fixed seed) mapped to their Collatz step
 * counts by 64 threads (the program's tasks), each mapping its slice into an array of its own (allocated by the main
 * thread as it starts the thread, so that 64 threads do not each make a malloc arena); the slices are joined in order
 * and copied into one array, and the result checked: its sum, its maximum and where it is first, a running hash of
 * the whole. */
#include "bench.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define TASKS 64

typedef struct {
  const int64_t *in;
  int64_t from, count; /* from: 1-based */
  int64_t *out;
} Slice;

static int64_t steps(int64_t x) {
  int64_t s = 0;
  while (x != 1) {
    x = x % 2 == 0 ? x / 2 : 3 * x + 1;
    ++s;
  }
  return s;
}

static void *map_slice(void *arg) {
  Slice *sl = arg;
  for (int64_t i = 1; i <= sl->count; ++i) sl->out[i - 1] = steps(sl->in[sl->from + i - 2]);
  return NULL;
}

int main(void) {
  int64_t n = 1000000, x = 777, pos = 1, sum = 0, max = 0, at = 0, chk = 0;
  int64_t *in = malloc((size_t)n * sizeof *in), *out = malloc((size_t)n * sizeof *out);
  for (int64_t i = 1; i <= n; ++i) {
    x = x * 16807 % 2147483647;
    in[i - 1] = x % 1000000 + 1;
  }
  long long t = bench_ms();
  int64_t chunk = n / TASKS;
  Slice slices[TASKS];
  pthread_t th[TASKS];
  int ntasks = 0;
  for (int64_t i = 1; i <= n; i += chunk) {
    slices[ntasks] = (Slice){in, i, chunk, malloc((size_t)chunk * sizeof(int64_t))};
    pthread_create(&th[ntasks], NULL, map_slice, &slices[ntasks]);
    ++ntasks;
  }
  for (int k = 0; k < ntasks; ++k) {
    pthread_join(th[k], NULL);
    memcpy(out + pos - 1, slices[k].out, (size_t)slices[k].count * sizeof *out);
    pos += slices[k].count;
    free(slices[k].out);
  }
  for (int64_t i = 1; i <= n; ++i) {
    x = out[i - 1];
    sum += x;
    if (x > max) {
      max = x;
      at = i;
    }
    chk = (chk * 31 + x) % 1000000007;
  }
  bench_report("parallel_map", t,
               pos == n + 1 && sum == 131359344 && max == 524 && at == 237731 && chk == 68222219);
  free(in);
  free(out);
  return 0;
}
