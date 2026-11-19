/* bench/radix_sort.prg in C: LSD radix sort, four byte passes, of 300,000 31-bit integers from an LCG, 10 times. */
#include "bench.h"
#include <stdlib.h>

int main(void) {
  long long t = bench_ms();
  int64_t n = 300000, sum = 0, cnt[257];
  int ok = 1;
  int64_t *a = malloc(sizeof(int64_t) * (size_t)(n + 1)), *b = malloc(sizeof(int64_t) * (size_t)(n + 1));
  for (int64_t rep = 1; rep <= 10; ++rep) {
    int64_t seed = rep + 30;
    for (int64_t i = 1; i <= n; ++i) {
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      a[i] = seed;
    }
    for (int64_t d = 0; d <= 3; ++d) {
      int64_t shift = d * 8;
      for (int64_t k = 1; k <= 256; ++k) cnt[k] = 0;
      for (int64_t i = 1; i <= n; ++i) cnt[((a[i] >> shift) & 0xFF) + 1]++;
      int64_t pos = 1;
      for (int64_t k = 1; k <= 256; ++k) {
        int64_t x = cnt[k];
        cnt[k] = pos;
        pos += x;
      }
      for (int64_t i = 1; i <= n; ++i) {
        int64_t x = a[i], k = ((x >> shift) & 0xFF) + 1;
        b[cnt[k]] = x;
        cnt[k]++;
      }
      int64_t *tmp = a;
      a = b;
      b = tmp;
    }
    for (int64_t i = 2; i <= n; ++i)
      if (a[i - 1] > a[i]) ok = 0;
    for (int64_t i = 1; i <= n; ++i) sum += a[i] * ((i & 7) + 1);
  }
  free(a);
  free(b);
  bench_report("radix_sort", t, ok && sum == 14501540638159929);
  return 0;
}
