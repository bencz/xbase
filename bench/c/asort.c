/* bench/asort.prg in C: the library's sort, 300,000 numbers ascending and 100,000 with a descending comparison,
 * 5 times. */
#include "bench.h"
#include <stdlib.h>

static int up(const void *p, const void *q) {
  int64_t a = *(const int64_t *)p, b = *(const int64_t *)q;
  return (a > b) - (a < b);
}

static int down(const void *p, const void *q) { return up(q, p); }

static int sort_both(void) {
  static int64_t a[300000], b[100000];
  int64_t x = 777, sum = 0;
  for (int i = 0; i < 300000; ++i) {
    x = (x * 16807) % 2147483647;
    a[i] = x % 1000000;
  }
  x = 4242;
  for (int i = 0; i < 100000; ++i) {
    x = (x * 16807) % 2147483647;
    b[i] = x % 1000000;
  }
  qsort(a, 300000, sizeof a[0], up);
  qsort(b, 100000, sizeof b[0], down);
  for (int i = 0; i < 300000; i += 1000) sum += a[i];
  return a[0] == 8 && a[149999] == 500673 && a[299999] == 999998 && sum == 149589926 && b[0] == 999994 &&
         b[49999] == 501282 && b[99999] == 8;
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 5; ++rep) ok = ok && sort_both();
  bench_report("asort", t, ok);
  return 0;
}
