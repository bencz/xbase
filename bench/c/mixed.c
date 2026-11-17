/* bench/mixed.prg in C: integers and doubles in one expression, double to integer, comparisons across both. */
#include "bench.h"

int main(void) {
  long long t = bench_ms(), n = 0;
  double s = 0;
  for (long long i = 1; i <= 20000000; ++i) {
    double d = (double)i + 3.14;
    s += (double)((long long)(d * 4) % 7);
    if (d > (double)(i + 3)) ++n;
    s += (double)i / 4;
  }
  bench_report("mixed", t, s == 50000062499998.0 && n == 20000000);
  return 0;
}
