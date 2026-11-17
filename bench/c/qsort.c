/* bench/qsort.prg in C: recursive quicksort over 200,000 Park-Miller numbers, 5 times. */
#include "bench.h"
#include <stdlib.h>

static void qs(int64_t *a, int64_t lo, int64_t hi) {
  while (lo < hi) {
    int64_t pivot = a[(lo + hi) / 2], i = lo, j = hi;
    while (i <= j) {
      while (a[i] < pivot) ++i;
      while (a[j] > pivot) --j;
      if (i <= j) {
        int64_t tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
        ++i;
        --j;
      }
    }
    if (j - lo < hi - i) {
      qs(a, lo, j);
      lo = i;
    } else {
      qs(a, i, hi);
      hi = j;
    }
  }
}

static int sort_random(void) {
  int64_t n = 200000, x = 12345, sum = 0;
  int64_t *a = malloc(sizeof(int64_t) * (size_t)(n + 1)); /* 1-based, as the xBase array */
  int sorted = 1;
  for (int64_t i = 1; i <= n; ++i) {
    x = (x * 16807) % 2147483647;
    a[i] = x % 1000000;
  }
  qs(a, 1, n);
  for (int64_t i = 2; i <= n; ++i)
    if (a[i - 1] > a[i]) sorted = 0;
  for (int64_t i = 1; i <= n; i += 1000) sum += a[i];
  int ok = sorted && a[1] == 6 && a[100000] == 500294 && a[n] == 999998 && sum == 99556483;
  free(a);
  return ok;
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 5; ++rep) ok = ok && sort_random();
  bench_report("qsort", t, ok);
  return 0;
}
