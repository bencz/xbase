/* Number helpers shared by the operators and the number functions. */
#ifndef XB_NUMERIC_H
#define XB_NUMERIC_H

#include "xbrt.h"

/* Text of a number in a field of `width` integer columns and `dec` decimals
 * (`width + 1 + dec` characters when dec > 0), Clipper style. */
xb_str *xb_num_to_str(const xb_item *it, int width, int dec);

static inline int xb_is_number(const xb_item *it) { return it->type == XB_T_INT || it->type == XB_T_DOUBLE; }

/* Whether a double is finite, and whether it is an infinity, by arithmetic:
 * the classification macros of some C libraries narrow their argument to
 * float on the way (a conversion warning). */
static inline int xb_dbl_finite(double d) { return d - d == 0.0; }
static inline int xb_dbl_infinite(double d) { return d == d && !xb_dbl_finite(d); }

/* A number's own field: its integer columns (its width, or the default for
 * its value) and its decimals. */
static inline void xb_num_field(const xb_item *it, int *width, int *dec) {
  if (it->type == XB_T_INT) {
    *width = it->width ? it->width : xb_num_int_width(it->v.i);
    *dec = 0;
  } else {
    *width = it->width ? it->width : xb_num_dbl_width(it->v.d);
    *dec = it->dec;
  }
}

/* A number item's value as a double. */
static inline double xb_num_value(const xb_item *it) { return it->type == XB_T_INT ? (double)it->v.i : it->v.d; }

/* Integer value of a number item; doubles are truncated, saturating at the
 * int64 range (NaN gives 0) instead of invoking undefined behavior. */
static inline int64_t xb_num_to_i64(const xb_item *it) {
  return it->type == XB_T_INT ? it->v.i : xb_dbl_to_i64(it->v.d);
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

/* a * b for sizes, saturated: a product past UINT64_MAX is UINT64_MAX, which
 * every size limit rejects. */
static inline uint64_t xb_mul_sat_u64(uint64_t a, uint64_t b) { return a && b > UINT64_MAX / a ? UINT64_MAX : a * b; }

/* A number truncated toward zero and wrapped to 32 bits signed (0 for a
 * double beyond 64 bits), as the C integers of the reference keep numbers. */
static inline int32_t xb_num_wrap_int32(const xb_item *n) {
  int64_t i = n->type == XB_T_INT ? n->v.i : xb_dbl_fits_i64(n->v.d) ? (int64_t)n->v.d : 0;
  uint32_t low = (uint32_t)i;
  return low < UINT32_C(0x80000000) ? (int32_t)low : (int32_t)((int64_t)low - (INT64_C(1) << 32));
}

#endif
