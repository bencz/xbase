/* bench/fannkuch_redux.prg in C: the pancake flips of every permutation of 9 elements (the checksum and the most
 * flips), 4 times. */
#include "bench.h"

enum { N = 9 };

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 4; ++rep) {
    int64_t perm[N], perm1[N], count[N], checksum = 0, maxFlips = 0, permCount = 0;
    int r = N;
    for (int i = 0; i < N; ++i) perm1[i] = i;
    for (;;) {
      while (r != 1) {
        count[r - 1] = r;
        --r;
      }
      for (int i = 0; i < N; ++i) perm[i] = perm1[i];
      int64_t flips = 0, k = perm[0];
      while (k != 0) {
        for (int64_t i = 0, j = k; i < j; ++i, --j) {
          int64_t x = perm[i];
          perm[i] = perm[j];
          perm[j] = x;
        }
        ++flips;
        k = perm[0];
      }
      if (flips > maxFlips) maxFlips = flips;
      checksum += permCount % 2 == 0 ? flips : -flips;
      for (;;) {
        if (r == N) goto done;
        int64_t perm0 = perm1[0];
        for (int i = 0; i < r; ++i) perm1[i] = perm1[i + 1];
        perm1[r] = perm0;
        --count[r];
        if (count[r] > 0) break;
        ++r;
      }
      ++permCount;
    }
  done:
    ok = ok && checksum == 8629 && maxFlips == 30;
  }
  bench_report("fannkuch_redux", t, ok);
  return 0;
}
