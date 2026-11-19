/* bench/julia_set.prg in C: the Julia set of c = -0.8 + 0.156i, escape times over a 1,600 x 1,200 grid, at most 255
 * iterations per point, a histogram folded into a checksum. */
#include "bench.h"

int main(void) {
  long long t = bench_ms();
  int64_t w = 1600, h = 1200, maxIter = 255, hist[256] = {0}, total = 0, cs = 0;
  double cr = -0.8, ci = 0.156;
  for (int64_t y = 0; y <= h - 1; ++y) {
    for (int64_t x = 0; x <= w - 1; ++x) {
      double zr = (double)x * 3.0 / (double)w - 1.5;
      double zi = (double)y * 2.0 / (double)h - 1.0;
      int64_t k = 0;
      while (k < maxIter) {
        double zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) break;
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
        ++k;
      }
      ++hist[k];
      total += k;
    }
  }
  for (int64_t k = 0; k <= maxIter; ++k) cs = (cs * 31 + hist[k]) % 2147483648;
  bench_report("julia_set", t, total == 69243973 && cs == 2105097618);
  return 0;
}
