/* bench/whetstone.prg in C: Whetstone's modules 2, 3, 4, 6, 7, 8, 9 and 11 scaled by a loop count of 5,000; the check
 * is the sum of the values each module leaves. Arrays are 1-based as in the .prg (element 0 unused). */
#include "bench.h"
#include <math.h>

static double t = 0.499975, t1 = 0.50025, t2 = 2.0, e1[5];
static int64_t j, k, l;

__attribute__((noinline)) static void pa(double *e) {
  for (int n = 0; n < 6; ++n) {
    e[1] = (e[1] + e[2] + e[3] - e[4]) * t;
    e[2] = (e[1] + e[2] - e[3] + e[4]) * t;
    e[3] = (e[1] - e[2] + e[3] + e[4]) * t;
    e[4] = (-e[1] + e[2] + e[3] + e[4]) / t2;
  }
}

__attribute__((noinline)) static void p0(void) {
  e1[j] = e1[k];
  e1[k] = e1[l];
  e1[l] = e1[j];
}

__attribute__((noinline)) static void p3(double x, double y, double *z) {
  double x1 = x, y1 = y;
  x1 = t * (x1 + y1);
  y1 = t * (x1 + y1);
  *z = (x1 + y1) / t2;
}

int main(void) {
  long long tm = bench_ms();
  int64_t nLoop = 5000;
  double x, y, z, check;
  e1[1] = 1.0;
  e1[2] = -1.0;
  e1[3] = -1.0;
  e1[4] = -1.0;
  for (int64_t i = 1; i <= 12 * nLoop; ++i) {
    e1[1] = (e1[1] + e1[2] + e1[3] - e1[4]) * t;
    e1[2] = (e1[1] + e1[2] - e1[3] + e1[4]) * t;
    e1[3] = (e1[1] - e1[2] + e1[3] + e1[4]) * t;
    e1[4] = (-e1[1] + e1[2] + e1[3] + e1[4]) * t;
  }
  check = e1[1] + e1[2] + e1[3] + e1[4];
  for (int64_t i = 1; i <= 14 * nLoop; ++i) pa(e1);
  check += e1[1] + e1[2] + e1[3] + e1[4];
  j = 1;
  for (int64_t i = 1; i <= 345 * nLoop; ++i) {
    j = j == 1 ? 2 : 3;
    j = j > 2 ? 0 : 1;
    j = j < 1 ? 1 : 0;
  }
  check += (double)j;
  j = 1;
  k = 2;
  l = 3;
  for (int64_t i = 1; i <= 210 * nLoop; ++i) {
    j = j * (k - j) * (l - k);
    k = l * k - (l - j) * k;
    l = (l - k) * (k + j);
    e1[l - 1] = (double)(j + k + l);
    e1[k - 1] = (double)(j * k * l);
  }
  check += (double)(j + k + l) + e1[1] + e1[2] + e1[3] + e1[4];
  x = 0.5;
  y = 0.5;
  for (int64_t i = 1; i <= 32 * nLoop; ++i) {
    x = t * atan(t2 * sin(x) * cos(x) / (cos(x + y) + cos(x - y) - 1.0));
    y = t * atan(t2 * sin(y) * cos(y) / (cos(x + y) + cos(x - y) - 1.0));
  }
  check += x + y;
  x = 1.0;
  y = 1.0;
  z = 1.0;
  for (int64_t i = 1; i <= 899 * nLoop; ++i) p3(x, y, &z);
  check += x + y + z;
  j = 1;
  k = 2;
  l = 3;
  e1[1] = 1.0;
  e1[2] = 2.0;
  e1[3] = 3.0;
  for (int64_t i = 1; i <= 616 * nLoop; ++i) p0();
  check += e1[1] + e1[2] + e1[3] + e1[4];
  x = 0.75;
  for (int64_t i = 1; i <= 93 * nLoop; ++i) x = sqrt(exp(log(x) / t1));
  check += x;
  bench_report("whetstone", tm, fabs(check - 30.95612841002271) < 0.000000001);
  return 0;
}
