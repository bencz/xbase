/* Exp() near 0 and Log() near 1, where the first steps' bounds shrink with |x| (and |x - 1|): their results are the
 * correctly rounded ones the double-double forms give, and their errors stay within those bounds; over random values
 * at every scale down to 2^-50 of the rows' width, and the neighbours of 0 and 1 a few units apart. */
#include "check.h"
#include "xbmath.h"

#include <stdint.h>

static uint64_t state = 88172645463325252u;

static uint64_t next(void) {
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return state;
}

static double exp_full(double x) {
  xb_dd y = {x, 0.0};
  int q;
  xb_dd v = xb_math_exp_parts(y, 0, &q);
  return xb_dd_scale(v, q);
}

static double log_full(double x) {
  xb_dd l = xb_math_log_dd(x);
  return l.hi + l.lo;
}

/* The first step's relative error at x, over its bound. */
static double exp_error_ratio(double x) {
  int q, qf;
  xb_dd y = {x, 0.0};
  xb_dd v = xb_math_exp_fast_parts(x, &q), full = xb_math_exp_parts(y, 0, &qf);
  xb_dd d = xb_dd_add(v, (xb_dd){-full.hi, -full.lo});
  return fabs(d.hi) / fabs(full.hi) / xb_math_exp_fast_err(x);
}

static double log_error_ratio(double x) {
  xb_dd v = xb_math_log_fast(x), full = xb_math_log_dd(x);
  xb_dd d = xb_dd_add(v, (xb_dd){-full.hi, -full.lo});
  return fabs(d.hi) / fabs(full.hi) / xb_math_log_fast_err(x);
}

int main(void) {
  for (int i = 0; i < 2000000; ++i) {
    double u = (double)(next() >> 11) * (1.0 / 9007199254740992.0) * 2.0 - 1.0; /* within [-1, 1) */
    double scale = ldexp(1.0, -(int)(next() % 50));
    double x = u * XB_MATH_EXP_ROW1 * scale, y = 1.0 + u * XB_MATH_LOG_ROW1 * scale;
    CHECK(xb_math_exp(x) == exp_full(x));
    CHECK(exp_error_ratio(x) < 0.5);
    if (y != 1.0) {
      CHECK(xb_math_log(y) == log_full(y));
      CHECK(log_error_ratio(y) < 0.5);
    }
  }
  for (int k = -200000; k <= 200000; ++k) {
    double y = 1.0 + (double)k * DBL_EPSILON / 2.0, x = (double)k * 1e-19;
    CHECK(xb_math_log(y) == log_full(y));
    CHECK(xb_math_exp(x) == exp_full(x));
  }
  return 0;
}
