/* bench/sieve.prg in C: the primes up to 2,000,000, 10 times. */
#include "bench.h"
#include <stdbool.h>
#include <stdlib.h>

int main(void) {
  long long t = bench_ms();
  int n = 2000000, ok = 1;
  bool *a = malloc((size_t)n + 1);
  for (int rep = 1; rep <= 10; ++rep) {
    int count = 0;
    for (int i = 2; i <= n; ++i) a[i] = true;
    a[1] = false;
    for (int i = 2; i * i <= n; ++i)
      if (a[i])
        for (int j = i * i; j <= n; j += i) a[j] = false;
    for (int i = 1; i <= n; ++i)
      if (a[i]) ++count;
    ok = ok && count == 148933;
  }
  free(a);
  bench_report("sieve", t, ok);
  return 0;
}
