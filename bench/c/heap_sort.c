/* bench/heap_sort.prg in C: heap sort in place of 300,000 integers from an LCG, 3 times. */
#include "bench.h"
#include <stdlib.h>

static void sift_down(int64_t *a, int64_t i, int64_t n) {
  int64_t x = a[i], c;
  while ((c = i * 2) <= n) {
    if (c < n && a[c + 1] > a[c]) ++c;
    if (a[c] <= x) break;
    a[i] = a[c];
    i = c;
  }
  a[i] = x;
}

int main(void) {
  long long t = bench_ms();
  int64_t n = 300000, sum = 0;
  int ok = 1;
  int64_t *a = malloc(sizeof(int64_t) * (size_t)(n + 1));
  for (int64_t rep = 1; rep <= 3; ++rep) {
    int64_t seed = rep + 10;
    for (int64_t i = 1; i <= n; ++i) {
      seed = seed * 16807 % 2147483647;
      a[i] = seed;
    }
    for (int64_t i = n >> 1; i >= 1; --i) sift_down(a, i, n);
    for (int64_t i = n; i >= 2; --i) {
      int64_t x = a[1];
      a[1] = a[i];
      a[i] = x;
      sift_down(a, 1, i - 1);
    }
    for (int64_t i = 2; i <= n; ++i)
      if (a[i - 1] > a[i]) ok = 0;
    for (int64_t i = 1; i <= n; ++i) sum += a[i] * ((i & 7) + 1);
  }
  free(a);
  bench_report("heap_sort", t, ok && sum == 4351668891394344);
  return 0;
}
