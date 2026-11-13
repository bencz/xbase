/* Number helpers shared by the operators and the number functions. */
#ifndef XB_NUMERIC_H
#define XB_NUMERIC_H

#include "xbrt.h"

/* Text of a number in a field of `width` integer columns and `dec` decimals
 * (`width + 1 + dec` characters when dec > 0), Clipper style. */
xb_str *xb_num_to_str(const xb_item *it, int width, int dec);
/* Str( n, nWidth, nDec ): the number in `width` characters in all, `dec` of them decimals (stars when it does not
 * fit, or when dec >= width); a database's numeric field holds this text. */
xb_str *xb_num_to_field(const xb_item *it, int width, int dec);
/* The same text written to `out` (`width` bytes; no NUL), nothing allocated for a number of up to 60 digits. */
void xb_num_to_field_buf(const xb_item *it, int width, int dec, char *out);
/* Val( c ) of the `len` bytes at `s`: the number, with the width and decimals its text gives. */
void xb_num_parse(const char *s, size_t len, xb_item *r);

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

/* The field a number shows in (display, QOut(), Transform() without a template, padding): its own, with SET FIXED
 * ON SET DECIMALS decimals, unless its width is 1 (as Clipper, which then keeps the number's). */
static inline void xb_num_shown_field(const xb_ctx *ctx, const xb_item *it, int *width, int *dec) {
  xb_num_field(it, width, dec);
  if (ctx->env->set_fixed && *width > 1) *dec = xb_decimals(ctx);
}

/* With SET FIXED ON, an integer that Transform() shows by its own field is that many columns wider, with no
 * decimals: two, and two per SET DECIMALS decimal (Clipper's field). */
static inline int xb_num_fixed_int_extra(const xb_ctx *ctx) { return 2 + 2 * (int)xb_decimals(ctx); }

/* 2^52: every double of this magnitude or more is an integer (the step between doubles is 1 or more there). */
#define XB_DBL_INTEGRAL 4503599627370496.0

/* A number item's value as a double. */
static inline double xb_num_value(const xb_item *it) { return it->type == XB_T_INT ? (double)it->v.i : it->v.d; }

/* Integer value of a number item; doubles are truncated, saturating at the
 * int64 range (NaN gives 0) instead of invoking undefined behavior. */
static inline int64_t xb_num_to_i64(const xb_item *it) {
  return it->type == XB_T_INT ? it->v.i : xb_dbl_to_i64(it->v.d);
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
