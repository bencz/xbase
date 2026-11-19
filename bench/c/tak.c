/* bench/tak.prg in C: the Takeuchi function Tak(24, 16, 8), 10 times. */
#include "bench.h"

static int64_t tak(int64_t x, int64_t y, int64_t z) {
  if (y < x) return tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y));
  return z;
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 10; ++rep) ok = ok && tak(24, 16, 8) == 9;
  bench_report("tak", t, ok);
  return 0;
}
