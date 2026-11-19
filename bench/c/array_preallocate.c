/* bench/array_preallocate.prg in C: 25 arrays of 2,000,000 numbers allocated at their size, filled by index, then
 * summed by index. */
#include "bench.h"
#include <stdlib.h>

int main(void) {
  long long t = bench_ms();
  int64_t n = 2000000, sum = 0, len = 0;
  for (int64_t r = 1; r <= 25; ++r) {
    int64_t *a = malloc((size_t)n * sizeof *a);
    for (int64_t i = 1; i <= n; ++i) a[i - 1] = i + r;
    for (int64_t i = 1; i <= n; ++i) sum += a[i - 1];
    len = n;
    free(a);
  }
  bench_report("array_preallocate", t, sum == 50000675000000LL && len == n);
  return 0;
}
