/* Number helpers shared by the operators and the number functions. */
#ifndef XB_NUMERIC_H
#define XB_NUMERIC_H

#include "xbrt.h"

/* Text of a number in a field of `width` integer columns and `dec` decimals
 * (`width + 1 + dec` characters when dec > 0), Clipper style. */
xb_str *xb_num_to_str(const xb_item *it, int width, int dec);

static inline int xb_is_number(const xb_item *it) { return it->type == XB_T_INT || it->type == XB_T_DOUBLE; }

/* A number item's value as a double. */
static inline double xb_num_value(const xb_item *it) { return it->type == XB_T_INT ? (double)it->v.i : it->v.d; }

/* Integer value of a number item; doubles are truncated, saturating at the
 * int64 range (NaN gives 0) instead of invoking undefined behavior. */
static inline int64_t xb_num_to_i64(const xb_item *it) {
  if (it->type == XB_T_INT) return it->v.i;
  double d = it->v.d;
  if (xb_dbl_fits_i64(d)) return (int64_t)d;
  if (d != d) return 0;
  return d > 0 ? INT64_MAX : INT64_MIN;
}

/* 64-bit integer arithmetic reporting overflow (returns non-zero) instead of
 * invoking undefined behavior. */
static inline int xb_add_overflow(int64_t a, int64_t b, int64_t *r) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_add_overflow(a, b, r);
#else
  if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return 1;
  *r = a + b;
  return 0;
#endif
}

static inline int xb_sub_overflow(int64_t a, int64_t b, int64_t *r) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_sub_overflow(a, b, r);
#else
  if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return 1;
  *r = a - b;
  return 0;
#endif
}

static inline int xb_mul_overflow(int64_t a, int64_t b, int64_t *r) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_mul_overflow(a, b, r);
#else
  if (a > 0 ? (b > 0 ? a > INT64_MAX / b : b < INT64_MIN / a)
            : (b > 0 ? a < INT64_MIN / b : (a != 0 && b < INT64_MAX / a)))
    return 1;
  *r = a * b;
  return 0;
#endif
}

#endif
