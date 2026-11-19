/* bench/counting_sort.prg in C: counting sort of 1,000,000 keys in 0 .. 65535 from an LCG, 6 times. */
#include "bench.h"
#include <stdlib.h>

int main(void) {
  long long t = bench_ms();
  int64_t n = 1000000, m = 65536, sum = 0;
  int ok = 1;
  int64_t *a = malloc(sizeof(int64_t) * (size_t)(n + 1)), *b = malloc(sizeof(int64_t) * (size_t)(n + 1));
  int64_t *cnt = malloc(sizeof(int64_t) * (size_t)(m + 1));
  for (int64_t rep = 1; rep <= 6; ++rep) {
    int64_t seed = rep + 20;
    for (int64_t i = 1; i <= n; ++i) {
      seed = seed * 16807 % 2147483647;
      a[i] = seed >> 15;
    }
    for (int64_t k = 1; k <= m; ++k) cnt[k] = 0;
    for (int64_t i = 1; i <= n; ++i) cnt[a[i] + 1]++;
    int64_t pos = 1;
    for (int64_t k = 1; k <= m; ++k) {
      int64_t c = cnt[k];
      cnt[k] = pos;
      pos += c;
    }
    for (int64_t i = 1; i <= n; ++i) {
      int64_t k = a[i] + 1;
      b[cnt[k]] = a[i];
      cnt[k]++;
    }
    for (int64_t i = 2; i <= n; ++i)
      if (b[i - 1] > b[i]) ok = 0;
    for (int64_t i = 1; i <= n; ++i) sum += b[i] * ((i & 7) + 1);
  }
  free(a);
  free(b);
  free(cnt);
  bench_report("counting_sort", t, ok && sum == 884633960242);
  return 0;
}
