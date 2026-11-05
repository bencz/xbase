/* Operators on dynamic values. Compiled code does the integer cases of +, -,
 * *, the comparisons and xb_for_test inline (compiler/src/codegen/FastPaths.cpp)
 * and calls these for every other case; both must give the same results. */
#include "xbrt_int.h"

#include <math.h>
#include <string.h>

static uint16_t num_dec(const xb_item *a) { return a->type == XB_T_DOUBLE ? a->dec : 0; }
static uint16_t max16(uint16_t a, uint16_t b) { return a > b ? a : b; }

static void arg_error(xb_ctx *ctx, int code, const char *op) {
  xb_rt_error(ctx, "BASE", code, "Argument error", op);
}

void xb_op_add(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (a->type == XB_T_INT && b->type == XB_T_INT) {
    int64_t v;
    if (!xb_add_overflow(a->v.i, b->v.i, &v)) { xb_set_int(r, v); return; }
    xb_set_double(r, (double)a->v.i + (double)b->v.i, 0);
  } else if (xb_is_number(a) && xb_is_number(b)) {
    xb_set_double(r, xb_num_value(a) + xb_num_value(b), max16(num_dec(a), num_dec(b)));
  } else if (a->type == XB_T_STRING && b->type == XB_T_STRING) {
    xb_set_str(r, xb_str_concat(a->v.s, b->v.s));
  } else if (a->type == XB_T_DATE && xb_is_number(b)) { /* the number is truncated to whole days */
    xb_set_date(r, xb_add_days(a->v.i, xb_num_to_i64(b)));
  } else if (xb_is_number(a) && b->type == XB_T_DATE) {
    xb_set_date(r, xb_add_days(b->v.i, xb_num_to_i64(a)));
  } else if (a->type == XB_T_DATE && b->type == XB_T_DATE) { /* Clipper adds the day numbers */
    xb_set_date(r, xb_add_days(a->v.i, b->v.i));
  } else {
    arg_error(ctx, 1081, "+");
  }
}

void xb_op_sub(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (a->type == XB_T_INT && b->type == XB_T_INT) {
    int64_t v;
    if (!xb_sub_overflow(a->v.i, b->v.i, &v)) { xb_set_int(r, v); return; }
    xb_set_double(r, (double)a->v.i - (double)b->v.i, 0);
  } else if (xb_is_number(a) && xb_is_number(b)) {
    xb_set_double(r, xb_num_value(a) - xb_num_value(b), max16(num_dec(a), num_dec(b)));
  } else if (a->type == XB_T_STRING && b->type == XB_T_STRING) {
    const xb_str *sa = a->v.s, *sb = b->v.s;
    xb_str *s = xb_str_alloc(sa->len + sb->len);
    xb_str_minus(s->data, sa->data, sa->len, sb->data, sb->len);
    xb_set_str(r, s);
  } else if (a->type == XB_T_DATE && b->type == XB_T_DATE) {
    int64_t v;
    if (xb_sub_overflow(a->v.i, b->v.i, &v)) xb_set_double(r, (double)a->v.i - (double)b->v.i, 0);
    else xb_set_int(r, v);
  } else if (a->type == XB_T_DATE && xb_is_number(b)) {
    xb_set_date(r, xb_add_days(a->v.i, (int64_t)(0 - (uint64_t)xb_num_to_i64(b))));
  } else {
    arg_error(ctx, 1082, "-");
  }
}

void xb_op_mul(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (a->type == XB_T_INT && b->type == XB_T_INT) {
    int64_t v;
    if (!xb_mul_overflow(a->v.i, b->v.i, &v)) { xb_set_int(r, v); return; }
    xb_set_double(r, (double)a->v.i * (double)b->v.i, xb_decimals(ctx)); /* unlike + and - */
  } else if (xb_is_number(a) && xb_is_number(b)) {
    xb_set_double(r, xb_num_value(a) * xb_num_value(b), (uint16_t)(num_dec(a) + num_dec(b)));
  } else {
    arg_error(ctx, 1083, "*");
  }
}

/* `/` and `%` always produce a double with SET DECIMALS decimals; an integer
 * result for exact division only comes from compile-time folding. */
void xb_op_div(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (!xb_is_number(a) || !xb_is_number(b)) { arg_error(ctx, 1084, "/"); return; }
  if (xb_num_value(b) == 0.0) { xb_rt_error(ctx, "BASE", 1340, "Zero divisor", "/"); return; }
  xb_set_double(r, xb_num_value(a) / xb_num_value(b), xb_decimals(ctx));
}

void xb_op_mod(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (!xb_is_number(a) || !xb_is_number(b)) { arg_error(ctx, 1085, "%"); return; }
  if (xb_num_value(b) == 0.0) { xb_rt_error(ctx, "BASE", 1341, "Zero divisor", "%"); return; }
  double v;
  if (a->type == XB_T_INT && b->type == XB_T_INT) v = b->v.i == -1 ? 0.0 : (double)(a->v.i % b->v.i);
  else v = fmod(xb_num_value(a), xb_num_value(b));
  xb_set_double(r, v, xb_decimals(ctx));
}

void xb_num_default_dec(xb_ctx *ctx, xb_item *r, double value) {
  xb_set_double(r, value, xb_decimals(ctx));
}

void xb_op_pow(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (!xb_is_number(a) || !xb_is_number(b)) { arg_error(ctx, 1088, "^"); return; }
  xb_set_double(r, pow(xb_num_value(a), xb_num_value(b)), xb_decimals(ctx));
}

void xb_op_neg(xb_ctx *ctx, xb_item *r, const xb_item *a) {
  if (a->type == XB_T_INT && a->v.i != INT64_MIN) {
    xb_set_int(r, -a->v.i);
  } else if (xb_is_number(a)) {
    xb_set_double(r, -xb_num_value(a), num_dec(a));
  } else {
    arg_error(ctx, 1080, "-");
  }
}

void xb_op_not(xb_ctx *ctx, xb_item *r, const xb_item *a) {
  if (a->type == XB_T_LOGICAL) xb_set_logical(r, !a->v.l);
  else arg_error(ctx, 1077, ".NOT.");
}

/* String comparison. Non-exact (SET EXACT OFF, operator =): when the right
 * operand is shorter, only its length is compared ("abc" = "ab" is .T.). */
static int str_cmp(const xb_str *a, const xb_str *b, int exact) {
  size_t la = a->len, lb = b->len;
  if (!exact && lb <= la) {
    int c = memcmp(a->data, b->data, lb);
    return c < 0 ? -1 : c > 0;
  }
  size_t n = la < lb ? la : lb;
  int c = memcmp(a->data, b->data, n);
  if (c) return c < 0 ? -1 : 1;
  if (la == lb) return 0;
  if (exact) { /* SET EXACT ON ignores trailing blanks */
    const xb_str *lng = la > lb ? a : b;
    size_t i = n;
    while (i < lng->len && lng->data[i] == ' ') ++i;
    if (i == lng->len) return 0;
  }
  return la < lb ? -1 : 1;
}

int xb_compare(const xb_ctx *ctx, const xb_item *a, const xb_item *b) {
  if (a->type == XB_T_INT && b->type == XB_T_INT)
    return (a->v.i > b->v.i) - (a->v.i < b->v.i);
  if (xb_is_number(a) && xb_is_number(b)) {
    double x = xb_num_value(a), y = xb_num_value(b);
    return (x > y) - (x < y);
  }
  if (a->type == XB_T_STRING && b->type == XB_T_STRING)
    return str_cmp(a->v.s, b->v.s, ctx->set_exact);
  if (a->type == XB_T_LOGICAL && b->type == XB_T_LOGICAL)
    return (a->v.l != 0) - (b->v.l != 0);
  if (a->type == XB_T_DATE && b->type == XB_T_DATE)
    return (a->v.i > b->v.i) - (a->v.i < b->v.i);
  return 2;
}

void xb_op_eq(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (a->type == XB_T_NIL || b->type == XB_T_NIL) { xb_set_logical(r, a->type == b->type); return; }
  int c = xb_compare(ctx, a, b);
  if (c == 2) arg_error(ctx, 1071, "=");
  else xb_set_logical(r, c == 0);
}

void xb_op_ne(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (a->type == XB_T_NIL || b->type == XB_T_NIL) { xb_set_logical(r, a->type != b->type); return; }
  int c = xb_compare(ctx, a, b);
  if (c == 2) arg_error(ctx, 1072, "<>");
  else xb_set_logical(r, c != 0);
}

void xb_op_exacteq(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if ((a->type == XB_T_ARRAY || a->type == XB_T_BLOCK) && a->type == b->type) { /* identity */
    xb_set_logical(r, a->v.p == b->v.p);
    return;
  }
  if (a->type == XB_T_STRING && b->type == XB_T_STRING) {
    const xb_str *x = a->v.s, *y = b->v.s;
    xb_set_logical(r, x->len == y->len && memcmp(x->data, y->data, x->len) == 0);
    return;
  }
  if (a->type == XB_T_NIL || b->type == XB_T_NIL) { xb_set_logical(r, a->type == b->type); return; }
  int c = xb_compare(ctx, a, b);
  if (c == 2) arg_error(ctx, 1070, "==");
  else xb_set_logical(r, c == 0);
}

#define XB_REL_OP(name, code, sym, test)                                         \
  void name(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {       \
    int c = xb_compare(ctx, a, b);                                               \
    if (c == 2) arg_error(ctx, code, sym);                                       \
    else xb_set_logical(r, test);                                                \
  }

XB_REL_OP(xb_op_lt, 1073, "<", c < 0)
XB_REL_OP(xb_op_le, 1074, "<=", c <= 0)
XB_REL_OP(xb_op_gt, 1075, ">", c > 0)
XB_REL_OP(xb_op_ge, 1076, ">=", c >= 0)

void xb_op_in(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (a->type != XB_T_STRING || b->type != XB_T_STRING) { arg_error(ctx, 1109, "$"); return; }
  const xb_str *x = a->v.s, *y = b->v.s;
  int found = 0;
  if (x->len == 0) found = 0; /* Clipper: "" $ "abc" is .F. */
  else if (x->len <= y->len) {
    for (size_t i = 0; i + x->len <= y->len; ++i)
      if (memcmp(y->data + i, x->data, x->len) == 0) { found = 1; break; }
  }
  xb_set_logical(r, found);
}

int32_t xb_cond_error(xb_ctx *ctx, const xb_item *a) {
  XB_UNUSED(a);
  arg_error(ctx, 1066, "conditional");
  return 0;
}

/* FOR test: a non-negative step loops while counter <= limit (fails with the
 * '>' error), a negative step while counter >= limit (the '<' error). */
int32_t xb_for_test(xb_ctx *ctx, const xb_item *counter, const xb_item *limit, const xb_item *step) {
  if (!xb_is_number(step)) {
    arg_error(ctx, 1073, "<");
    return 0;
  }
  int back = xb_num_value(step) < 0;
  int c = xb_compare(ctx, counter, limit);
  if (c == 2) {
    arg_error(ctx, back ? 1073 : 1075, back ? "<" : ">");
    return 0;
  }
  return back ? c >= 0 : c <= 0;
}
