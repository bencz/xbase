/* xbrt - internal declarations, not part of the compiler ABI. */
#ifndef XBRT_INT_H
#define XBRT_INT_H

#include "xbrt.h"

/* Item frames live in a segmented stack: a segment is never reallocated,
 * so pointers handed to compiled code stay valid for the frame lifetime.
 * Frames are GC roots (GC itself comes later). */
typedef struct xb_seg {
  struct xb_seg *prev;
  struct xb_seg *next;      /* spare segment kept for reuse */
  size_t         cap;
  size_t         used;
  xb_item        items[];
} xb_seg;

struct xb_ctx {
  uint32_t pending;         /* [ABI] must stay the first field */
  int32_t  exitcode;
  xb_seg  *seg;
  /* Later: memvar (PRIVATE) stack, current work area, SETs, ErrorBlock -
   * all per task, so they travel with a Task across threads like .NET's
   * ExecutionContext. */
  int32_t  set_decimals;
  int      set_exact;
};

#define XB_SEG_DEFAULT 4096

/* Functions with the fixed xBase calling convention do not use every parameter. */
#define XB_UNUSED(x) ((void)(x))

/* Uninitialized string of `len` bytes (plus the terminating NUL). */
xb_str *xb_str_alloc(size_t len);

/* Text of a number in a field of `width` integer columns and `dec` decimals
 * (`width + 1 + dec` characters when dec > 0), Clipper style. */
xb_str *xb_num_to_str(const xb_item *it, int width, int dec);

/* SET DECIMALS as an item's decimal count (items keep 16 bits, as in Clipper). */
static inline uint16_t xb_decimals(const xb_ctx *ctx) { return (uint16_t)ctx->set_decimals; }

/* Integer value of a number item; doubles are truncated, saturating at the
 * int64 range (NaN gives 0) instead of invoking undefined behavior. */
static inline int64_t xb_num_to_i64(const xb_item *it) {
  if (it->type == XB_T_INT) return it->v.i;
  double d = it->v.d;
  if (d != d) return 0;
  if (d >= 9223372036854775807.0) return INT64_MAX;
  if (d <= -9223372036854775808.0) return INT64_MIN;
  return (int64_t)d;
}

/* One-time process setup that differs between operating systems. */
void xb_platform_init(void);

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
