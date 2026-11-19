/* bench/binary_search.prg in C: 1,000,000 keys from a fixed-seed generator looked for by binary search in a sorted
 * array of 1,000,000 numbers (every third number). */
#include "bench.h"
#include <stdlib.h>

static int64_t seed = 4711;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

int main(void) {
  int64_t n = 1000000, found = 0, possum = 0;
  int64_t *a = malloc((size_t)n * sizeof *a);
  for (int64_t i = 1; i <= n; ++i) a[i - 1] = i * 3;
  long long t = bench_ms();
  for (int64_t i = 1; i <= 1000000; ++i) {
    int64_t k = rnd(3000010), lo = 1, hi = n;
    while (lo <= hi) {
      int64_t mid = (lo + hi) / 2;
      if (a[mid - 1] < k)
        lo = mid + 1;
      else if (a[mid - 1] > k)
        hi = mid - 1;
      else {
        ++found;
        possum += mid;
        break;
      }
    }
  }
  bench_report("binary_search", t, found == 333181 && possum == 166512579113LL);
  free(a);
  return 0;
}
