/* bench/dgemm.prg in C: C := alpha A B + beta C on 250 x 250 matrices of doubles, row-major, loops in i, k, j order,
 * 3 times; the check is the sum and the trace of C. */
#include "bench.h"
#include <math.h>
#include <stdlib.h>

int main(void) {
  long long t = bench_ms();
  int64_t n = 250, seed = 7;
  double alpha = 1.5, beta = 0.5, sum = 0.0, trace = 0.0;
  double *a = malloc(sizeof(double) * (size_t)(n * n)), *b = malloc(sizeof(double) * (size_t)(n * n));
  double *c = malloc(sizeof(double) * (size_t)(n * n));
  for (int64_t i = 0; i < n * n; ++i) {
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    a[i] = (double)seed / 1073741824.0 - 1.0;
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    b[i] = (double)seed / 1073741824.0 - 1.0;
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    c[i] = (double)seed / 1073741824.0 - 1.0;
  }
  for (int rep = 1; rep <= 3; ++rep)
    for (int64_t i = 0; i < n; ++i) {
      double *ci = c + i * n;
      for (int64_t j = 0; j < n; ++j) ci[j] = beta * ci[j];
      for (int64_t k = 0; k < n; ++k) {
        double aik = alpha * a[i * n + k];
        const double *bk = b + k * n;
        for (int64_t j = 0; j < n; ++j) ci[j] += aik * bk[j];
      }
    }
  for (int64_t i = 0; i < n; ++i) {
    for (int64_t j = 0; j < n; ++j) sum += c[i * n + j];
    trace += c[i * n + i];
  }
  int ok = fabs(sum + 145.62810535074) < 0.000001 && fabs(trace - 342.80906789597) < 0.000001;
  free(a);
  free(b);
  free(c);
  bench_report("dgemm", t, ok);
  return 0;
}
