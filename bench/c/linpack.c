/* bench/linpack.prg in C: a dense 300 x 300 system (LINPACK's matrix generator) solved by LU factorization with
 * partial pivoting (DGEFA, DGESL, through DAXPY, IDAMAX and DSCAL on one column-major array), 5 times; the check is
 * the solution, all ones, and the residual of A x - b. Indices are 1-based as in the .prg (element 0 unused). */
#include "bench.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void matgen(double *a, double *b, int64_t n) {
  int64_t init = 1325;
  for (int64_t j = 1; j <= n; ++j)
    for (int64_t i = 1; i <= n; ++i) {
      init = (3125 * init) % 65536;
      a[(j - 1) * n + i] = ((double)init - 32768.0) / 16384.0;
    }
  for (int64_t i = 1; i <= n; ++i) b[i] = 0.0;
  for (int64_t j = 1; j <= n; ++j)
    for (int64_t i = 1; i <= n; ++i) b[i] += a[(j - 1) * n + i];
}

static void daxpy(int64_t n, double da, const double *dx, int64_t x, double *dy, int64_t y) {
  if (n > 0 && da != 0.0)
    for (int64_t i = 0; i <= n - 1; ++i) dy[y + i] += da * dx[x + i];
}

static void dscal(int64_t n, double da, double *dx, int64_t x) {
  for (int64_t i = x; i <= x + n - 1; ++i) dx[i] = da * dx[i];
}

static int64_t idamax(int64_t n, const double *dx, int64_t x) {
  int64_t imax = 1;
  double dmax = fabs(dx[x]);
  for (int64_t i = 2; i <= n; ++i) {
    double v = fabs(dx[x + i - 1]);
    if (v > dmax) {
      imax = i;
      dmax = v;
    }
  }
  return imax;
}

static void dgefa(double *a, int64_t n, int64_t *ipvt) {
  for (int64_t k = 1; k <= n - 1; ++k) {
    int64_t col = (k - 1) * n;
    int64_t l = idamax(n - k + 1, a, col + k) + k - 1;
    ipvt[k] = l;
    if (a[col + l] != 0.0) {
      double t;
      if (l != k) {
        t = a[col + l];
        a[col + l] = a[col + k];
        a[col + k] = t;
      }
      t = -1.0 / a[col + k];
      dscal(n - k, t, a, col + k + 1);
      for (int64_t j = k + 1; j <= n; ++j) {
        t = a[(j - 1) * n + l];
        if (l != k) {
          a[(j - 1) * n + l] = a[(j - 1) * n + k];
          a[(j - 1) * n + k] = t;
        }
        daxpy(n - k, t, a, col + k + 1, a, (j - 1) * n + k + 1);
      }
    }
  }
  ipvt[n] = n;
}

static void dgesl(const double *a, int64_t n, const int64_t *ipvt, double *b) {
  for (int64_t k = 1; k <= n - 1; ++k) {
    int64_t l = ipvt[k];
    double t = b[l];
    if (l != k) {
      b[l] = b[k];
      b[k] = t;
    }
    daxpy(n - k, t, a, (k - 1) * n + k + 1, b, k + 1);
  }
  for (int64_t k = n; k >= 1; --k) {
    b[k] = b[k] / a[(k - 1) * n + k];
    double t = -b[k];
    daxpy(k - 1, t, a, (k - 1) * n + 1, b, 1);
  }
}

int main(void) {
  long long t = bench_ms();
  int64_t n = 300;
  int ok = 1;
  double *a = malloc(sizeof(double) * (size_t)(n * n + 1)), *b = malloc(sizeof(double) * (size_t)(n + 1));
  double *x = malloc(sizeof(double) * (size_t)(n + 1));
  int64_t *ipvt = malloc(sizeof(int64_t) * (size_t)(n + 1));
  for (int rep = 1; rep <= 5; ++rep) {
    matgen(a, b, n);
    memcpy(x, b, sizeof(double) * (size_t)(n + 1));
    dgefa(a, n, ipvt);
    dgesl(a, n, ipvt, x);
    /* the factorization overwrote a: a new copy gives the residual */
    matgen(a, b, n);
    double maxErr = 0.0, resid = 0.0;
    for (int64_t i = 1; i <= n; ++i) {
      maxErr = fmax(maxErr, fabs(x[i] - 1.0));
      double s = -b[i];
      for (int64_t j = 1; j <= n; ++j) s += a[(j - 1) * n + i] * x[j];
      resid = fmax(resid, fabs(s));
    }
    ok = ok && maxErr < 0.000000001 && resid < 0.000000001;
  }
  free(a);
  free(b);
  free(x);
  free(ipvt);
  bench_report("linpack", t, ok);
  return 0;
}
