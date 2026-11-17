/* bench/loops.prg in C: the sum of the first 500,000,000 odd numbers. */
#include "bench.h"

int main(void) {
  long long t = bench_ms();
  volatile long long limit = 500000000; /* not folded away at compile time */
  long long n = 0;
  for (long long i = 1; i <= limit; ++i) n = n + i * 2 - 1;
  bench_report("loops", t, n == 250000000000000000LL);
  return 0;
}
