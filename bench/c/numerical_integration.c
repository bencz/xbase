/* bench/numerical_integration.prg in C: Simpson's rule over 1,000,000 intervals for four integrals whose values are
 * known (4 / (1 + x^2) and sqrt(x) on [0, 1], exp(-x^2) on [0, 1], 1 / x on [1, 2]), each function called through a
 * pointer, 3 times. */
#include "bench.h"
#include <math.h>

static double quarterCircle(double x) { return 4.0 / (1.0 + x * x); }
static double gauss(double x) { return exp(-(x * x)); }
static double inverse(double x) { return 1.0 / x; }
static double root(double x) { return sqrt(x); }

/* noipa: the calls through the pointer stay calls, as the .prg's Eval() of a codeblock. */
__attribute__((noipa)) static double simpson(double (*f)(double), double a, double b, int64_t n) {
  double h = (b - a) / (double)n, s = f(a) + f(b);
  for (int64_t i = 1; i < n; ++i) s = s + (i % 2 == 1 ? 4.0 : 2.0) * f(a + (double)i * h);
  return s * h / 3.0;
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 3; ++rep) {
    int64_t n = 1000000;
    double r1 = simpson(quarterCircle, 0.0, 1.0, n), r2 = simpson(gauss, 0.0, 1.0, n);
    double r3 = simpson(inverse, 1.0, 2.0, n), r4 = simpson(root, 0.0, 1.0, n);
    ok = ok && fabs(r1 - 3.141592653589793) < 1e-12 && fabs(r2 - 0.746824132812427) < 1e-12 &&
         fabs(r3 - 0.6931471805599453) < 1e-12 && fabs(r4 - 2.0 / 3.0) < 1e-9;
  }
  bench_report("numerical_integration", t, ok);
  return 0;
}
