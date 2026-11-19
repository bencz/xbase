/* bench/black_scholes.prg in C: Black-Scholes prices of 500,000 European options (a call and a put each), their
 * inputs from a linear congruential generator, the sum of the prices. */
#include "bench.h"
#include <math.h>

/* The cumulative normal distribution (the polynomial approximation of Abramowitz and Stegun, 26.2.17). */
static double cnd(double d) {
  double k = 1.0 / (1.0 + 0.2316419 * fabs(d));
  double w = 1.0 - 0.3989422804014327 * exp(-0.5 * d * d) * k *
                       (0.319381530 + k * (-0.356563782 + k * (1.781477937 + k * (-1.821255978 + k * 1.330274429))));
  return d < 0.0 ? 1.0 - w : w;
}

int main(void) {
  long long t = bench_ms();
  int64_t n = 500000, seed = 12345;
  double r = 0.02, v = 0.30, sumCall = 0.0, sumPut = 0.0;
  for (int64_t i = 1; i <= n; ++i) {
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    double s = 5.0 + 25.0 * (double)seed / 2147483648.0;
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    double x = 1.0 + 99.0 * (double)seed / 2147483648.0;
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    double tm = 0.25 + 9.75 * (double)seed / 2147483648.0;
    double vst = v * sqrt(tm);
    double d1 = (log(s / x) + (r + v * v * 0.5) * tm) / vst;
    double d2 = d1 - vst;
    double disc = x * exp(-r * tm);
    sumCall += s * cnd(d1) - disc * cnd(d2);
    sumPut += disc * cnd(-d2) - s * cnd(-d1);
  }
  int ok = fabs(sumCall / 1494483.92385431 - 1) < 0.000000001 && fabs(sumPut / 15576648.71605097 - 1) < 0.000000001;
  bench_report("black_scholes", t, ok);
  return 0;
}
