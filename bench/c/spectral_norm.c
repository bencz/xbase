/* bench/spectral_norm.prg in C: the spectral norm of A(i,j) = 1 / ((i+j)(i+j+1)/2 + i + 1) cut to 1,200 x 1,200, 10
 * rounds of the power method, each element of A from a function call. */
#include "bench.h"
#include <math.h>
#include <stdlib.h>

__attribute__((noinline)) static double eval_a(int64_t i, int64_t j) {
  return 1.0 / (double)((i + j) * (i + j + 1) / 2 + i + 1);
}

static void mul_av(int64_t n, const double *u, double *v) {
  for (int64_t i = 0; i < n; ++i) {
    double s = 0.0;
    for (int64_t j = 0; j < n; ++j) s += eval_a(i, j) * u[j];
    v[i] = s;
  }
}

static void mul_atv(int64_t n, const double *u, double *v) {
  for (int64_t i = 0; i < n; ++i) {
    double s = 0.0;
    for (int64_t j = 0; j < n; ++j) s += eval_a(j, i) * u[j];
    v[i] = s;
  }
}

static void mul_atav(int64_t n, const double *u, double *v, double *w) {
  mul_av(n, u, w);
  mul_atv(n, w, v);
}

int main(void) {
  long long t = bench_ms();
  int64_t n = 1200;
  double *u = malloc(sizeof(double) * (size_t)n), *v = malloc(sizeof(double) * (size_t)n);
  double *w = malloc(sizeof(double) * (size_t)n), vbv = 0.0, vv = 0.0;
  for (int64_t i = 0; i < n; ++i) {
    u[i] = 1.0;
    v[i] = 0.0;
    w[i] = 0.0;
  }
  for (int i = 1; i <= 10; ++i) {
    mul_atav(n, u, v, w);
    mul_atav(n, v, u, w);
  }
  for (int64_t i = 0; i < n; ++i) {
    vbv += u[i] * v[i];
    vv += v[i] * v[i];
  }
  double r = sqrt(vbv / vv);
  free(u);
  free(v);
  free(w);
  bench_report("spectral_norm", t, fabs(r - 1.2742241501) < 0.000000001);
  return 0;
}
