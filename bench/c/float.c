/* bench/float.prg in C: Mandelbrot iterations over a 250 x 250 grid, at most 100 per point, 6 times. */
#include "bench.h"

int main(void) {
  long long t = bench_ms(), total = 0;
  int size = 250, max_iter = 100;
  for (int rep = 1; rep <= 6; ++rep)
    for (int y = 0; y < size; ++y) {
      double ci = y * 2.0 / size - 1.0;
      for (int x = 0; x < size; ++x) {
        double cr = x * 3.0 / size - 2.0, zr = 0.0, zi = 0.0;
        int k = 0;
        while (k < max_iter) {
          double zr2 = zr * zr, zi2 = zi * zi;
          if (zr2 + zi2 > 4.0) break;
          zi = 2.0 * zr * zi + ci;
          zr = zr2 - zi2 + cr;
          ++k;
        }
        total += k;
      }
    }
  bench_report("float", t, total == 11391384);
  return 0;
}
