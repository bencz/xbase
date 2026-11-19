/* bench/gaussian_elimination.prg in C: a 300 x 300 system of random doubles (a fixed seed) whose solution is all ones,
 * solved by Gaussian elimination with partial pivoting and back substitution, 6 times. */
#include "bench.h"
#include <math.h>

enum { N = 300 };

int main(void) {
  long long t = bench_ms();
  static double store[N][N + 1];
  double *a[N], x[N];
  int ok = 1;
  for (int rep = 1; rep <= 6; ++rep) {
    int64_t seed = 42;
    for (int i = 0; i < N; ++i) {
      double s = 0.0;
      a[i] = store[i];
      for (int j = 0; j < N; ++j) {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        a[i][j] = (double)seed / 2147483648.0 - 0.5;
        s = s + a[i][j];
      }
      a[i][N] = s;
    }
    for (int k = 0; k < N; ++k) {
      int p = k;
      for (int i = k + 1; i < N; ++i)
        if (fabs(a[i][k]) > fabs(a[p][k])) p = i;
      double *row = a[k];
      a[k] = a[p];
      a[p] = row;
      for (int i = k + 1; i < N; ++i) {
        double f = a[i][k] / a[k][k];
        for (int j = k; j <= N; ++j) a[i][j] = a[i][j] - f * a[k][j];
      }
    }
    double err = 0.0;
    for (int i = N - 1; i >= 0; --i) {
      double s = a[i][N];
      for (int j = i + 1; j < N; ++j) s = s - a[i][j] * x[j];
      x[i] = s / a[i][i];
      if (fabs(x[i] - 1.0) > err) err = fabs(x[i] - 1.0);
    }
    ok = ok && err < 1e-9;
  }
  bench_report("gaussian_elimination", t, ok);
  return 0;
}
