/*
 * xbrt - xbase runtime: public ABI shared with the xbc compiler.
 *
 * Everything in this header that the compiler mirrors is marked [ABI].
 * The compiler never hard-codes sizes or offsets: it describes these
 * structures as LLVM types and lets the target DataLayout lay them out,
 * exactly as the C compiler does for the runtime. At startup the program's
 * entry (xb_app_main) calls xb_abi_check() so a mismatch is caught instead of
 * corrupting memory.
 *
 * [ABI] Every 32-bit parameter of an entry point used by compiled code is
 * uint32_t and every 32-bit result is int32_t: the compiler relies on this to
 * apply the target's argument-extension rules.
 *
 * Endianness rule: in-memory structures are always native; on-disk formats
 * (DBF, NTX, CDX, ...) are only ever read/written through explicit
 * little/big-endian helpers, never by copying structs.
 */
#ifndef XBRT_H
#define XBRT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XB_ABI_VERSION 2u

/* [ABI] item type tags */
typedef enum xb_type {
  XB_T_NIL     = 0,
  XB_T_LOGICAL = 1,
  XB_T_INT     = 2,   /* numeric, integral (int64)        */
  XB_T_DOUBLE  = 3,   /* numeric, floating point           */
  XB_T_STRING  = 4,
  XB_T_COUNT_
} xb_type;

/* [ABI] string header. Strings are immutable: they can be shared freely
 * between items and threads. `data` is always NUL terminated (len excludes
 * the terminator) so it can be handed to C without copying. */
#define XB_STR_STATIC 0x1u   /* emitted by the compiler, never freed */

typedef struct xb_str xb_str;
#ifndef __cplusplus
/* The compiler mirrors this layout as an LLVM type and never uses the C++ view
 * of it (a flexible array member is not standard C++). */
struct xb_str {
  uint32_t flags;
  uint32_t hash;
  size_t   len;
  char     data[];
};
#endif

/* [ABI] the dynamic value. Copying an item is a plain memcpy: memory is
 * managed by a tracing GC, so there is no reference counting. */
typedef struct xb_item {
  uint32_t type;      /* xb_type                                    */
  uint16_t width;     /* numeric display width (0 = default)        */
  uint16_t dec;       /* numeric decimals                           */
  union {
    int32_t  l;       /* XB_T_LOGICAL                               */
    int64_t  i;       /* XB_T_INT                                   */
    double   d;       /* XB_T_DOUBLE                                */
    xb_str  *s;       /* XB_T_STRING                                */
    void    *p;
  } v;
} xb_item;

/* [ABI] `pending` is tested by compiled code after every call that can
 * fail. Non-zero means: stop and unwind to the nearest handler (or out of
 * the program). It is also how BREAK, QUIT and GC safepoint requests reach
 * compiled code - one predictable branch covers all of them, works on every
 * target, and survives AWAIT (unlike setjmp/longjmp). */
enum {
  XB_PENDING_NONE  = 0,
  XB_PENDING_ERROR = 1u << 0,
  XB_PENDING_BREAK = 1u << 1,
  XB_PENDING_QUIT  = 1u << 2,
};

typedef struct xb_ctx xb_ctx;   /* per-task context; first field is `pending` [ABI] */

/* [ABI] dynamic calling convention used by every xBase-callable function:
 * arguments are an array of items owned by the caller, the result is
 * written to `ret`, which the caller sets to NIL before the call. */
typedef void (*xb_func)(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

void     xb_abi_check(uint32_t version, uint32_t item_size, uint32_t str_hdr_size);

xb_item *xb_frame_push(xb_ctx *ctx, uint32_t nslots);
void     xb_frame_pop(xb_ctx *ctx, xb_item *base);
void     xb_args_copy(xb_item *dst, uint32_t nparams, uint32_t argc, const xb_item *argv);

void     xb_op_add(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_sub(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_mul(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_div(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_mod(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_pow(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_neg(xb_ctx *ctx, xb_item *r, const xb_item *a);
void     xb_op_eq(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);      /* =  */
void     xb_op_exacteq(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b); /* == */
void     xb_op_ne(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_lt(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_le(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_gt(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_ge(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);
void     xb_op_in(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b);      /* $  */
void     xb_op_not(xb_ctx *ctx, xb_item *r, const xb_item *a);

/* A double whose decimals are the current SET DECIMALS (constant-folded `/`,
 * `%` on doubles and `^`). */
void     xb_num_default_dec(xb_ctx *ctx, xb_item *r, double value);

/* Non-logical value used as a condition: raises the error, returns 0. */
int32_t  xb_cond_error(xb_ctx *ctx, const xb_item *a);
/* FOR ... TO ... STEP test: 1 = keep looping. */
int32_t  xb_for_test(xb_ctx *ctx, const xb_item *counter, const xb_item *limit, const xb_item *step);

/* Reports a runtime error ("Error <subsystem>/<code>  <desc>: <operation>")
 * and raises XB_PENDING_ERROR. Errors are fatal until ErrorBlock() exists. */
void     xb_rt_error(xb_ctx *ctx, const char *subsystem, int code, const char *desc, const char *operation);

xb_ctx  *xb_ctx_new(void);
void     xb_ctx_free(xb_ctx *ctx);
uint32_t xb_ctx_pending(const xb_ctx *ctx);
void     xb_ctx_raise(xb_ctx *ctx, uint32_t what);
int      xb_ctx_exitcode(const xb_ctx *ctx);

xb_str  *xb_str_new(const char *p, size_t len);
xb_str  *xb_str_concat(const xb_str *a, const xb_str *b);

static inline void xb_set_nil(xb_item *it) {
  it->type = XB_T_NIL;
  it->width = 0;
  it->dec = 0;
  it->v.i = 0;
}
static inline void xb_set_logical(xb_item *it, int b) {
  xb_set_nil(it);
  it->type = XB_T_LOGICAL;
  it->v.l = b ? 1 : 0;
}
static inline void xb_set_int(xb_item *it, int64_t n) {
  xb_set_nil(it);
  it->type = XB_T_INT;
  it->v.i = n;
}
static inline void xb_set_double(xb_item *it, double d, uint16_t dec) {
  xb_set_nil(it);
  it->type = XB_T_DOUBLE;
  it->dec = dec;
  it->v.d = d;
}
static inline void xb_set_str(xb_item *it, xb_str *s) {
  xb_set_nil(it);
  it->type = XB_T_STRING;
  it->v.s = s;
}

/* Default display width of the integer part of a number (Clipper rule):
 * 10 columns, 20 when the value does not fit in 10. Shared by the compiler
 * (numeric literals) and the runtime (computed values). */
static inline uint16_t xb_num_int_width(int64_t i) {
  return (uint16_t)((i < -999999999 || i > INT64_C(9999999999)) ? 20 : 10);
}
static inline uint16_t xb_num_dbl_width(double d) {
  return (uint16_t)((d > 9999999999.0 || d < -999999999.0) ? 20 : 10);
}

/* Display width of an integer constant in the program (a literal or a folded
 * expression): 10 columns plus one per digit beyond nine, 20 for values of
 * -1000000000 or less. Computed integers use xb_num_int_width instead. */
static inline uint16_t xb_num_const_int_width(int64_t i) {
  if (i <= -1000000000) return 20;
  uint16_t w = 10;
  for (; i >= 1000000000; i /= 10) ++w;
  return w;
}

/* Clipper's string `-`: the trailing blanks of `a` move after `b`. Writes
 * la + lb bytes to `out`. */
static inline void xb_str_minus(char *out, const char *a, size_t la, const char *b, size_t lb) {
  size_t keep = la;
  while (keep && a[keep - 1] == ' ') --keep;
  for (size_t i = 0; i < keep; ++i) out[i] = a[i];
  for (size_t i = 0; i < lb; ++i) out[keep + i] = b[i];
  for (size_t i = keep + lb; i < la + lb; ++i) out[i] = ' ';
}

/* Formats a value the way ? / QOut() shows it. Returns a new string. */
xb_str  *xb_item_display(const xb_item *it);

/* Program entry, emitted by xbc in the start module. */
void     xb_app_main(xb_ctx *ctx);

#ifdef __cplusplus
}
#endif
#endif /* XBRT_H */
