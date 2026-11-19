/* bench/merge_sort.prg in C: recursive top-down merge sort of 300,000 integers from an LCG, 3 times. */
#include "bench.h"
#include <stdlib.h>

static void merge_sort(int64_t *a, int64_t *w, int64_t lo, int64_t hi) {
  if (hi - lo < 1) return;
  int64_t mid = (lo + hi) >> 1;
  merge_sort(a, w, lo, mid);
  merge_sort(a, w, mid + 1, hi);
  int64_t i = lo, j = mid + 1, k = lo;
  while (i <= mid && j <= hi) {
    if (a[j] < a[i])
      w[k++] = a[j++];
    else
      w[k++] = a[i++];
  }
  while (i <= mid) w[k++] = a[i++];
  while (j <= hi) w[k++] = a[j++];
  for (k = lo; k <= hi; ++k) a[k] = w[k];
}

int main(void) {
  long long t = bench_ms();
  int64_t n = 300000, sum = 0;
  int ok = 1;
  int64_t *a = malloc(sizeof(int64_t) * (size_t)(n + 1)), *w = malloc(sizeof(int64_t) * (size_t)(n + 1));
  for (int64_t rep = 1; rep <= 3; ++rep) {
    int64_t seed = rep;
    for (int64_t i = 1; i <= n; ++i) {
      seed = seed * 16807 % 2147483647;
      a[i] = seed;
    }
    merge_sort(a, w, 1, n);
    for (int64_t i = 2; i <= n; ++i)
      if (a[i - 1] > a[i]) ok = 0;
    for (int64_t i = 1; i <= n; ++i) sum += a[i] * ((i & 7) + 1);
  }
  free(a);
  free(w);
  bench_report("merge_sort", t, ok && sum == 4349496350073402);
  return 0;
}
