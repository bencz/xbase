/* bench/random_massivo.prg in C: 30,000,000 rounds of four random-number generators (LCG, 64-bit xorshift,
 * multiply-with-carry, 32-bit xorshift), their sums, a histogram of their top bytes and a scaled sum. */
#include "bench.h"
#include <math.h>

int main(void) {
  long long t = bench_ms();
  int64_t n = 30000000, hist[257] = {0}, lcg = 12345, z = 362436069, w = 521288629, xs32 = 2463534242;
  int64_t s1 = 0, s2 = 0, s3 = 0, s4 = 0, check = 0, r;
  uint64_t xs64 = 88172645463325252ULL;
  double fsum = 0.0;
  for (int64_t i = 1; i <= n; ++i) {
    lcg = (lcg * 1103515245 + 12345) & 0x7FFFFFFF;
    s1 += lcg;
    hist[(lcg >> 23) + 1]++;
    fsum += (double)lcg / 2147483648.0;

    xs64 ^= xs64 << 13;
    xs64 ^= xs64 >> 7;
    xs64 ^= xs64 << 17;
    r = (int64_t)((xs64 >> 32) & 0xFFFFFFFF);
    s2 += r;
    hist[(r >> 24) + 1]++;

    z = 36969 * (z & 0xFFFF) + (z >> 16);
    w = 18000 * (w & 0xFFFF) + (w >> 16);
    r = ((z << 16) + w) & 0xFFFFFFFF;
    s3 += r;
    hist[(r >> 24) + 1]++;

    xs32 ^= (xs32 << 13) & 0xFFFFFFFF;
    xs32 ^= xs32 >> 17;
    xs32 ^= (xs32 << 5) & 0xFFFFFFFF;
    s4 += xs32;
    hist[(xs32 >> 24) + 1]++;
  }
  for (int i = 1; i <= 256; ++i) check += hist[i] * i;
  int ok = s1 == 32217797789519680 && s2 == 64428429046975605 && s3 == 64423232821249815 &&
           s4 == 64428119329011698 && check == 15421035017 && fabs(fsum - 15002581.187856009) < 0.000001;
  bench_report("random_massivo", t, ok);
  return 0;
}
