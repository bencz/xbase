/* bench/mandelbrot.prg in C: the Mandelbrot set as a bitmap, 1,600 x 1,600 points, at most 50 iterations each, the
 * bytes folded into a checksum. */
#include "bench.h"

int main(void) {
  long long t = bench_ms();
  int64_t n = 1600, cs = 0, nBytes = 0;
  for (int64_t y = 0; y <= n - 1; ++y) {
    double ci = 2.0 * (double)y / (double)n - 1.0;
    int64_t bits = 0, nBits = 0;
    for (int64_t x = 0; x <= n - 1; ++x) {
      double cr = 2.0 * (double)x / (double)n - 1.5;
      double zr = 0.0, zi = 0.0, tr = 0.0, ti = 0.0;
      int k = 0;
      while (k < 50 && tr + ti <= 4.0) {
        zi = 2.0 * zr * zi + ci;
        zr = tr - ti + cr;
        tr = zr * zr;
        ti = zi * zi;
        ++k;
      }
      bits = (bits << 1) | (tr + ti <= 4.0 ? 1 : 0);
      ++nBits;
      if (nBits == 8 || x == n - 1) {
        bits = bits << (8 - nBits);
        cs = (cs * 31 + bits) & 0x7FFFFFFF;
        ++nBytes;
        bits = 0;
        nBits = 0;
      }
    }
  }
  bench_report("mandelbrot", t, cs == 366763831 && nBytes == n * ((n + 7) / 8));
  return 0;
}
