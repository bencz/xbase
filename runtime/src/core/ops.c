/* Operators on dynamic values. Compiled code does the number cases of +, -, *,
 * /, %, the comparisons and xb_for_test inline (compiler/src/codegen/
 * FastPaths.cpp) and calls these for every other case; both must give the same
 * results. The compiler's type inference (compiler/src/xir/Types.cpp,
 * binaryResult) mirrors the types these operators produce: a change here that
 * lets an operator produce a new type must change it too. */
#include "xbrt_int.h"

#include <math.h>
#include <string.h>

static uint16_t num_dec(const xb_item *a) { return a->type == XB_T_DOUBLE ? a->dec : 0; }
static uint16_t max16(uint16_t a, uint16_t b) { return a > b ? a : b; }

/* The argument error of a binary operator; its handler may substitute the
 * result. */
static void arg_error2(xb_ctx *ctx, xb_item *r, int32_t code, const char *op, const xb_item *a, const xb_item *b) {
  xb_item args[2] = {*a, *b};
  xb_arg_error(ctx, r, code, op, 2, args);
}

static void zero_divisor(xb_ctx *ctx, xb_item *r, int32_t code, const char *op, const xb_item *a, const xb_item *b) {
  xb_item args[2] = {*a, *b};
  xb_error(ctx, r, EG_ZERODIV, code, op, XB_ERR_SUBSTITUTE, 2, args);
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
    arg_error2(ctx, r, 1081, "+", a, b);
  }
}

void xb_op_append(xb_ctx *ctx, xb_item *var, const xb_item *b) {
  if (var->type != XB_T_STRING || b->type != XB_T_STRING) {
    if (var->type == XB_T_STRING) xb_str_share(var->v.s); /* the error's arguments see it */
    xb_item sum; /* the variable changes only if the + gives a value: an error that unwinds leaves it */
    xb_set_nil(&sum);
    xb_gc_root_push(ctx, &sum);
    xb_op_add(ctx, &sum, var, b);
    if (!(ctx->pending && xb_pending(ctx))) *var = sum;
    xb_gc_root_pop(ctx, 1);
    return;
  }
  xb_str *s = var->v.s;
  const xb_str *t = b->v.s;
  if ((s->flags & XB_STR_APPEND) && t->len <= s->hash - s->len) { /* also done in place by compiled code */
    memcpy(s->data + s->len, t->data, t->len);
    s->len += t->len;
    s->data[s->len] = '\0';
    return;
  }
  uint64_t len = (uint64_t)s->len + t->len;
  if (len <= 1) { /* a shared string */
    xb_set_str(var, xb_str_concat(s, t));
    return;
  }
  xb_str *r = xb_str_growing(len, len < 16 ? 32 : 2 * len); /* doubling: appends cost O(1) amortized */
  memcpy(r->data, s->data, s->len);
  memcpy(r->data + s->len, t->data, t->len);
  xb_set_str(var, r);
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
    arg_error2(ctx, r, 1082, "-", a, b);
  }
}

/* ++ and --: + and - with 1 where they apply, else their own error. */
static void step_by_one(xb_ctx *ctx, xb_item *r, const xb_item *a, int up) {
  if (!xb_is_number(a) && a->type != XB_T_DATE) {
    xb_item args[1] = {*a};
    xb_arg_error(ctx, r, up ? 1086 : 1087, up ? "++" : "--", 1, args);
    return;
  }
  xb_item one;
  xb_set_int(&one, 1);
  if (up) xb_op_add(ctx, r, a, &one);
  else xb_op_sub(ctx, r, a, &one);
}

void xb_op_inc(xb_ctx *ctx, xb_item *r, const xb_item *a) { step_by_one(ctx, r, a, 1); }
void xb_op_dec(xb_ctx *ctx, xb_item *r, const xb_item *a) { step_by_one(ctx, r, a, 0); }

void xb_op_mul(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (a->type == XB_T_INT && b->type == XB_T_INT) {
    int64_t v;
    if (!xb_mul_overflow(a->v.i, b->v.i, &v)) { xb_set_int(r, v); return; }
    xb_set_double(r, (double)a->v.i * (double)b->v.i, xb_decimals(ctx)); /* unlike + and - */
  } else if (xb_is_number(a) && xb_is_number(b)) {
    xb_set_double(r, xb_num_value(a) * xb_num_value(b), (uint16_t)(num_dec(a) + num_dec(b)));
  } else {
    arg_error2(ctx, r, 1083, "*", a, b);
  }
}

/* `/` and `%` always produce a double with SET DECIMALS decimals; an integer
 * result for exact division only comes from compile-time folding. */
void xb_op_div(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (!xb_is_number(a) || !xb_is_number(b)) { arg_error2(ctx, r, 1084, "/", a, b); return; }
  if (xb_num_value(b) == 0.0) { zero_divisor(ctx, r, 1340, "/", a, b); return; }
  xb_set_double(r, xb_num_value(a) / xb_num_value(b), xb_decimals(ctx));
}

void xb_op_mod(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (!xb_is_number(a) || !xb_is_number(b)) { arg_error2(ctx, r, 1085, "%", a, b); return; }
  if (xb_num_value(b) == 0.0) { zero_divisor(ctx, r, 1341, "%", a, b); return; }
  double v;
  if (a->type == XB_T_INT && b->type == XB_T_INT) v = b->v.i == -1 ? 0.0 : (double)(a->v.i % b->v.i);
  else v = fmod(xb_num_value(a), xb_num_value(b));
  xb_set_double(r, v, xb_decimals(ctx));
}

void xb_num_default_dec(xb_ctx *ctx, xb_item *r, double value) {
  xb_set_double(r, value, xb_decimals(ctx));
}

void xb_op_pow(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (!xb_is_number(a) || !xb_is_number(b)) { arg_error2(ctx, r, 1088, "^", a, b); return; }
  xb_set_double(r, pow(xb_num_value(a), xb_num_value(b)), xb_decimals(ctx));
}

void xb_op_neg(xb_ctx *ctx, xb_item *r, const xb_item *a) {
  if (a->type == XB_T_INT && a->v.i != INT64_MIN) {
    xb_set_int(r, -a->v.i);
  } else if (xb_is_number(a)) {
    xb_set_double(r, -xb_num_value(a), num_dec(a));
  } else {
    xb_arg_error(ctx, r, 1080, "-", 1, a);
  }
}

void xb_op_not(xb_ctx *ctx, xb_item *r, const xb_item *a) {
  if (a->type == XB_T_LOGICAL) xb_set_logical(r, !a->v.l);
  else xb_arg_error(ctx, r, 1077, ".NOT.", 1, a);
}

/* The bit operators (xb_bit_apply and xb_bit_function in xbrt.h): numbers
 * are taken as integers, truncated and saturated to 64 bits (NaN is 0), as
 * the bit functions take them; anything else is the argument error of the
 * operator, with the code of its operation. `b` is NULL for ~. */
static void bit_op(xb_ctx *ctx, xb_item *r, xb_bit_op op, const xb_item *a, const xb_item *b) {
  const xb_bit_function *f = xb_bit_function_of(op);
  if (!xb_is_number(a) || (b && !xb_is_number(b))) {
    if (b) arg_error2(ctx, r, f->code, f->symbol, a, b);
    else xb_arg_error(ctx, r, f->code, f->symbol, 1, a);
    return;
  }
  xb_set_int(r, xb_bit_apply(op, xb_num_to_i64(a), b ? xb_num_to_i64(b) : 0));
}

void xb_op_bitand(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) { bit_op(ctx, r, XB_BIT_AND, a, b); }
void xb_op_bitor(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) { bit_op(ctx, r, XB_BIT_OR, a, b); }
void xb_op_bitxor(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) { bit_op(ctx, r, XB_BIT_XOR, a, b); }
void xb_op_shl(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) { bit_op(ctx, r, XB_BIT_SHL, a, b); }
void xb_op_shr(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) { bit_op(ctx, r, XB_BIT_SAR, a, b); }
void xb_op_bitnot(xb_ctx *ctx, xb_item *r, const xb_item *a) { bit_op(ctx, r, XB_BIT_NOT, a, NULL); }

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
  if (xb_is_number(a) && xb_is_number(b)) return xb_order_double(xb_num_value(a), xb_num_value(b));
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
  if (c == 2) arg_error2(ctx, r, 1071, "=", a, b);
  else xb_set_logical(r, xb_order_is(c, 0, 1, 0));
}

void xb_op_ne(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (a->type == XB_T_NIL || b->type == XB_T_NIL) { xb_set_logical(r, a->type != b->type); return; }
  int c = xb_compare(ctx, a, b);
  if (c == 2) arg_error2(ctx, r, 1072, "<>", a, b);
  else xb_set_logical(r, !xb_order_is(c, 0, 1, 0));
}

void xb_op_exacteq(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  int identity = a->type == XB_T_ARRAY || a->type == XB_T_BLOCK || a->type == XB_T_OBJECT || a->type == XB_T_HASH;
  if (identity && a->type == b->type) {
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
  if (c == 2) arg_error2(ctx, r, 1070, "==", a, b);
  else xb_set_logical(r, xb_order_is(c, 0, 1, 0));
}

#define XB_REL_OP(name, code, sym, less, equal, greater)                          \
  void name(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {       \
    int c = xb_compare(ctx, a, b);                                               \
    if (c == 2) arg_error2(ctx, r, code, sym, a, b);                             \
    else xb_set_logical(r, xb_order_is(c, less, equal, greater));                \
  }

XB_REL_OP(xb_op_lt, 1073, "<", 1, 0, 0)
XB_REL_OP(xb_op_le, 1074, "<=", 1, 1, 0)
XB_REL_OP(xb_op_gt, 1075, ">", 0, 0, 1)
XB_REL_OP(xb_op_ge, 1076, ">=", 0, 1, 1)

/* `$`: a substring of a string, or a key of a hash. */
void xb_op_in(xb_ctx *ctx, xb_item *r, const xb_item *a, const xb_item *b) {
  if (b->type == XB_T_HASH && xb_hash_key_ok(a)) {
    xb_set_logical(r, xb_hash_find(b->v.p, a) >= 0);
    return;
  }
  if (a->type != XB_T_STRING || b->type != XB_T_STRING) { arg_error2(ctx, r, 1109, "$", a, b); return; }
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
  xb_error(ctx, NULL, EG_ARG, 1066, "conditional", 0, 1, a);
  return 0;
}

/* The comparison a substituted result stands for: a logical, else the error
 * of a condition. */
static int32_t substituted_truth(xb_ctx *ctx, const xb_item *v) {
  if (v->type == XB_T_LOGICAL) return v->v.l != 0;
  return xb_cond_error(ctx, v);
}

/* FOR test: a non-negative step loops while counter <= limit, that is until
 * counter > limit (whose error is the '>' one), a negative step until counter
 * < limit (the '<' error); a substituted result is that comparison's. A
 * non-numeric step is the '<' error between the step and 0, whose substituted
 * result tells whether the step goes back. */
int32_t xb_for_test(xb_ctx *ctx, const xb_item *counter, const xb_item *limit, const xb_item *step) {
  int back;
  xb_item r;
  if (xb_is_number(step)) {
    back = xb_num_value(step) < 0;
  } else {
    xb_item zero;
    xb_set_int(&zero, 0);
    arg_error2(ctx, &r, 1073, "<", step, &zero);
    if (ctx->pending & XB_PENDING_UNWIND) return 0;
    back = substituted_truth(ctx, &r);
    if (ctx->pending & XB_PENDING_UNWIND) return 0;
  }
  int c = xb_compare(ctx, counter, limit);
  if (c != 2) return back ? xb_order_is(c, 0, 1, 1) : xb_order_is(c, 1, 1, 0); /* a NaN ends the loop */
  arg_error2(ctx, &r, back ? 1073 : 1075, back ? "<" : ">", counter, limit);
  if (ctx->pending & XB_PENDING_UNWIND) return 0;
  int32_t past = substituted_truth(ctx, &r);
  return !past && !(ctx->pending & XB_PENDING_UNWIND);
}
