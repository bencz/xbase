/* bench/ackermann.prg in C: Ackermann's function A(3, 9), deep recursion, 3 times. */
#include "bench.h"

static int64_t ack(int64_t m, int64_t n) {
  if (m == 0) return n + 1;
  if (n == 0) return ack(m - 1, 1);
  return ack(m - 1, ack(m, n - 1));
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 3; ++rep) ok = ok && ack(3, 9) == 4093;
  bench_report("ackermann", t, ok);
  return 0;
}
