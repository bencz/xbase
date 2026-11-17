/* bench/fib.prg in C: recursive calls, Fib( 37 ). */
#include "bench.h"

static int64_t fib(int64_t n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

int main(void) {
  long long t = bench_ms();
  int64_t r = fib(37);
  bench_report("fib", t, r == 24157817);
  return 0;
}
