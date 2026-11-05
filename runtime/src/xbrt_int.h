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
  uint16_t set_decimals;
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
