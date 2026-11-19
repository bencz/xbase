/* bench/knapsack.prg in C: 0/1 knapsack by dynamic programming, 200 items into capacities up to 10,000, 10 times. */
#include "bench.h"

int main(void) {
  long long t = bench_ms();
  enum { N = 200, CAP = 10000 };
  static int64_t best[CAP + 2], w[N + 1], v[N + 1];
  int64_t total = 0;
  for (int64_t rep = 1; rep <= 10; ++rep) {
    int64_t seed = rep;
    for (int i = 1; i <= N; ++i) {
      seed = seed * 16807 % 2147483647;
      w[i] = (seed >> 16) % 1000 + 1;
      seed = seed * 16807 % 2147483647;
      v[i] = (seed >> 16) % 1000 + 1;
    }
    for (int c = 1; c <= CAP + 1; ++c) best[c] = 0;
    for (int i = 1; i <= N; ++i) {
      int64_t wi = w[i], vi = v[i];
      for (int64_t c = CAP + 1; c >= wi + 1; --c) {
        int64_t x = best[c - wi] + vi;
        if (x > best[c]) best[c] = x;
      }
    }
    total += best[CAP + 1];
  }
  bench_report("knapsack", t, total == 366516);
  return 0;
}
