/* Number functions and number/text conversion. */
#include "xbrt_int.h"

#include <float.h>
#include <math.h>
#include <string.h>

#define XB_STR_MAX_WIDTH 90 /* widest field Str() takes from an item */

/* Every argument passed, even NIL, must be a number. */
static int str_args_ok(uint32_t argc, const xb_item *argv) {
  for (uint32_t i = 0; i < argc && i < 3; ++i)
    if (!xb_is_number(&argv[i])) return 0;
  return argc >= 1;
}

/* The text of Str( n [, nWidth [, nDec]] ). A field too narrow for its
 * decimals is all stars. NULL with `*overflow` set when the width asked is
 * past the widest field (INT32_MAX, which the formatter takes), NULL alone
 * when no memory holds it (a size the program chose). */
static xb_str *str_of(uint32_t argc, const xb_item *argv, int *overflow) {
  int width, dec;
  *overflow = 0;
  xb_num_field(&argv[0], &width, &dec);
  if (width > XB_STR_MAX_WIDTH) width = XB_STR_MAX_WIDTH;
  int hasWidth = argc > 1;
  if (hasWidth) {
    int64_t w = xb_num_to_i64(&argv[1]);
    if (w > INT32_MAX) {
      *overflow = 1;
      return NULL;
    }
    width = w < 1 ? xb_num_int_width(0) : (int)w;
    dec = 0;
  }
  if (width > 1 && argc > 2) {
    int64_t d = xb_num_to_i64(&argv[2]);
    dec = d <= 0 ? 0 : (d > INT32_MAX ? INT32_MAX : (int)d);
  }
  if (!hasWidth) return xb_num_to_str(&argv[0], width, dec);
  xb_str *s = xb_str_try_alloc((size_t)width);
  if (s) xb_num_to_field_buf(&argv[0], width, dec, s->data);
  return s;
}

/* Str()'s text made: else BASE/1234 String overflow for a width past the widest field, BASE/9032 Memory low for one
 * that memory does not hold. */
static int str_made(xb_ctx *ctx, xb_item *ret, const char *function, xb_str *s, int overflow, uint32_t argc,
                    xb_item *argv) {
  if (s) return 1;
  if (overflow) xb_error_overflow(ctx, ret, function, argc, argv);
  else xb_error_memory(ctx, ret, function, argc, argv);
  return 0;
}

/* Str( n [, nWidth [, nDec]] ): nWidth is the whole field (with the dot and
 * the decimals); without it, the number's own field is used. */
void xbf_STR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (!str_args_ok(argc, argv)) {
    xb_fn_arg_error(ctx, ret, 1099, "STR", argc, argv);
    return;
  }
  int overflow;
  xb_str *s = str_of(argc, argv, &overflow);
  if (str_made(ctx, ret, "STR", s, overflow, argc, argv)) xb_set_str(ret, s);
}

/* StrZero(): Str() with the leading blanks (after a sign) turned into zeros. */
void xbf_STRZERO(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (!str_args_ok(argc, argv)) {
    xb_fn_arg_error(ctx, ret, 6003, "STRZERO", argc, argv);
    return;
  }
  int overflow;
  xb_str *s = str_of(argc, argv, &overflow);
  if (!str_made(ctx, ret, "STRZERO", s, overflow, argc, argv)) return;
  xb_set_str(ret, s);
  size_t i = 0;
  int neg = 0;
  while (i < s->len && (s->data[i] == ' ' || s->data[i] == '-')) {
    neg |= s->data[i] == '-';
    s->data[i++] = '0';
  }
  if (neg && s->len) s->data[0] = '-';
}

/* Val( c ): leading blanks, a sign, digits and a dot. The display width
 * counts the blanks, the sign and every character before the dot; the
 * decimals count every character after it (Clipper rules). */
void xbf_VAL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || argv[0].type != XB_T_STRING) {
    xb_fn_arg_error(ctx, ret, 1098, "VAL", argc, argv);
    return;
  }
  xb_num_parse(argv[0].v.s->data, argv[0].v.s->len, ret);
}

/* Int( n ): the integer part, with n's display width (a field's integer columns, a computed number's own). */
void xbf_INT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, 1090, "INT", argc, argv);
    return;
  }
  if (argv[0].type == XB_T_INT) {
    *ret = argv[0];
    return;
  }
  double t = trunc(argv[0].v.d);
  uint16_t width = argv[0].width ? argv[0].width : xb_num_dbl_width(argv[0].v.d);
  if (xb_dbl_fits_i64(t)) xb_set_int(ret, (int64_t)t);
  else xb_set_double(ret, t, 0);
  ret->width = width;
}

void xbf_ABS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, 1089, "ABS", argc, argv);
    return;
  }
  if (argv[0].type == XB_T_INT && argv[0].v.i >= 0) {
    *ret = argv[0]; /* the integer as it is: its display width stays */
    return;
  }
  if (argv[0].type == XB_T_INT && argv[0].v.i != INT64_MIN) xb_set_int(ret, -argv[0].v.i);
  else xb_set_double(ret, fabs(xb_num_value(&argv[0])), argv[0].type == XB_T_DOUBLE ? argv[0].dec : 0);
}

/* Round( n, nDec ): xb_num_round. Int() truncates the value as it is. */
void xbf_ROUND(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 2 || !xb_is_number(&argv[0]) || !xb_is_number(&argv[1])) {
    xb_fn_arg_error(ctx, ret, 1094, "ROUND", argc, argv);
    return;
  }
  int64_t d = xb_num_to_i64(&argv[1]);
  double r = xb_num_round(xb_num_value(&argv[0]), d);
  if (d <= 0 && xb_dbl_fits_i64(r)) xb_set_int(ret, (int64_t)r);
  else xb_set_double(ret, r, (uint16_t)(d <= 0 ? 0 : d > UINT16_MAX ? UINT16_MAX : d));
}

static void minmax(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int wantMax, const char *fn) {
  if (argc < 2 || !((xb_is_number(&argv[0]) && xb_is_number(&argv[1])) ||
                    (argv[0].type == XB_T_DATE && argv[1].type == XB_T_DATE))) {
    xb_fn_arg_error(ctx, ret, wantMax ? 1093 : 1092, fn, argc, argv);
    return;
  }
  double a = argv[0].type == XB_T_DATE ? (double)argv[0].v.i : xb_num_value(&argv[0]);
  double b = argv[1].type == XB_T_DATE ? (double)argv[1].v.i : xb_num_value(&argv[1]);
  *ret = (wantMax ? a >= b : a <= b) ? argv[0] : argv[1];
}

void xbf_MAX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { minmax(ctx, ret, argc, argv, 1, "MAX"); }
void xbf_MIN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { minmax(ctx, ret, argc, argv, 0, "MIN"); }

/* Results outside the reals follow Clipper: Sqrt() of a negative number and
 * an Exp() that overflows give 0, Log() of a negative number gives the same
 * as Log( 0 ), minus infinity (shown as stars). */
double xb_exp(double x) { return xb_math_exp(x); }
double xb_log(double x) { return xb_math_log(x); }

static double math_sqrt(double x) { return x < 0 ? 0.0 : sqrt(x); }
static double math_exp(double x) {
  double r = xb_math_exp(x);
  return xb_dbl_infinite(r) ? 0.0 : r;
}
static double math_log(double x) { return x < 0 ? -HUGE_VAL : xb_math_log(x); }

static void math1(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, double (*f)(double), int code,
                  const char *fn) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, code, fn, argc, argv);
    return;
  }
  xb_set_double(ret, f(xb_num_value(&argv[0])), xb_decimals(ctx));
}

void xbf_SQRT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, math_sqrt, 1097, "SQRT");
}
void xbf_EXP(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, math_exp, 1096, "EXP");
}
void xbf_LOG(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, math_log, 1095, "LOG");
}

/* The CA-Tools math functions (core/trig.c): a double with SET DECIMALS decimals, a NaN or an infinity (shown as
 * stars) outside their domain, as in CA-Tools; a missing or non-numeric argument is an argument error (CA-Tools gives
 * 0), with xbase's own codes from 9036. */
enum { CODE_SIN = 9036, CODE_COS, CODE_TAN, CODE_COT, CODE_ASIN, CODE_ACOS, CODE_ATAN, CODE_ATN2, CODE_SINH,
       CODE_COSH, CODE_TANH, CODE_LOG10, CODE_DTOR, CODE_RTOD, CODE_FLOOR, CODE_CEILING, CODE_SIGN, CODE_FACT };

void xbf_SIN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_sin, CODE_SIN, "SIN");
}
void xbf_COS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_cos, CODE_COS, "COS");
}
void xbf_TAN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_tan, CODE_TAN, "TAN");
}
void xbf_COT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_cot, CODE_COT, "COT");
}
void xbf_ASIN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_asin, CODE_ASIN, "ASIN");
}
void xbf_ACOS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_acos, CODE_ACOS, "ACOS");
}
void xbf_ATAN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_atan, CODE_ATAN, "ATAN");
}
void xbf_SINH(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_sinh, CODE_SINH, "SINH");
}
void xbf_COSH(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_cosh, CODE_COSH, "COSH");
}
void xbf_TANH(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_tanh, CODE_TANH, "TANH");
}
void xbf_LOG10(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_log10, CODE_LOG10, "LOG10");
}
void xbf_DTOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_dtor, CODE_DTOR, "DTOR");
}
void xbf_RTOD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  math1(ctx, ret, argc, argv, xb_rtod, CODE_RTOD, "RTOD");
}

/* Atn2( nY, nX ): C's atan2( y, x ). */
void xbf_ATN2(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 2 || !xb_is_number(&argv[0]) || !xb_is_number(&argv[1])) {
    xb_fn_arg_error(ctx, ret, CODE_ATN2, "ATN2", argc, argv);
    return;
  }
  xb_set_double(ret, xb_atan2(xb_num_value(&argv[0]), xb_num_value(&argv[1])), xb_decimals(ctx));
}

/* Pi(): the double nearest pi. */
void xbf_PI(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_double(ret, XB_PI, xb_decimals(ctx));
}

/* Floor( n ) and Ceiling( n ): an integer (a double with no decimals past 64 bits). */
static void rounded(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, int up, int code, const char *fn) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, code, fn, argc, argv);
    return;
  }
  if (argv[0].type == XB_T_INT) {
    xb_set_int(ret, argv[0].v.i);
    return;
  }
  double r = up ? ceil(argv[0].v.d) : floor(argv[0].v.d);
  if (xb_dbl_fits_i64(r)) xb_set_int(ret, (int64_t)r);
  else xb_set_double(ret, r, 0);
}
void xbf_FLOOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  rounded(ctx, ret, argc, argv, 0, CODE_FLOOR, "FLOOR");
}
void xbf_CEILING(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  rounded(ctx, ret, argc, argv, 1, CODE_CEILING, "CEILING");
}

/* Sign( n ): -1, 0 or 1 (0 for a NaN). */
void xbf_SIGN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, CODE_SIGN, "SIGN", argc, argv);
    return;
  }
  double v = xb_num_value(&argv[0]);
  xb_set_int(ret, v > 0 ? 1 : v < 0 ? -1 : 0);
}

/* Fact( n ): xb_fact, a double with SET DECIMALS decimals. */
void xbf_FACT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, CODE_FACT, "FACT", argc, argv);
    return;
  }
  xb_set_double(ret, xb_fact(xb_num_value(&argv[0])), xb_decimals(ctx));
}

/* The bit functions (xb_bit_apply and xb_bit_function in xbrt.h): numbers are
 * taken as integers (truncated, saturated to 64 bits; NaN is 0); anything else,
 * or a missing argument, is an argument error. */
static void bit_function(xb_ctx *ctx, xb_item *ret, uint32_t argc, const xb_item *argv, xb_bit_op op) {
  const xb_bit_function *f = xb_bit_function_of(op);
  int64_t v[2] = {0, 0};
  for (uint32_t k = 0; k < f->arity; ++k) {
    if (k >= argc || !xb_is_number(&argv[k])) {
      xb_fn_arg_error(ctx, ret, f->code, f->name, argc, argv);
      return;
    }
    v[k] = xb_num_to_i64(&argv[k]);
  }
  xb_set_int(ret, xb_bit_apply(f->op, v[0], v[1]));
}

void xbf_BITAND(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  bit_function(ctx, ret, argc, argv, XB_BIT_AND);
}
void xbf_BITOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  bit_function(ctx, ret, argc, argv, XB_BIT_OR);
}
void xbf_BITXOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  bit_function(ctx, ret, argc, argv, XB_BIT_XOR);
}
void xbf_BITNOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  bit_function(ctx, ret, argc, argv, XB_BIT_NOR);
}
void xbf_BITNOT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  bit_function(ctx, ret, argc, argv, XB_BIT_NOT);
}
void xbf_BITSHL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  bit_function(ctx, ret, argc, argv, XB_BIT_SHL);
}
void xbf_BITSHR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  bit_function(ctx, ret, argc, argv, XB_BIT_SHR);
}

/* Mod( nNumber, nBase ): the remainder with the sign of nBase (dBase's
 * modulus), a number with SET DECIMALS decimals. A zero base is error 1341,
 * after which the number comes back as it is, whatever the block returned. */
void xbf_MOD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 2 || !xb_is_number(&argv[0]) || !xb_is_number(&argv[1])) {
    xb_arg_error(ctx, ret, 1085, "%", argc < 2 ? argc : 2, argv);
    return;
  }
  double n = xb_num_value(&argv[0]), base = xb_num_value(&argv[1]);
  if (base == 0.0) {
    if (xb_error(ctx, ret, EG_ZERODIV, 1341, "%", XB_ERR_SUBSTITUTE, 2, argv)) *ret = argv[0];
    return;
  }
  double r = fmod(n, base);
  if (r != 0.0 && (n > 0 ? base < 0 : base > 0)) r += base;
  xb_set_double(ret, r, xb_decimals(ctx));
}

/* Descend( xValue ): a value that sorts the other way: each byte of a string
 * as 256 minus it (0 stays 0), a number negated, a date as a number counting
 * down, a logical negated; NIL for anything else. */
void xbf_DESCEND(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1) return;
  const xb_item *v = &argv[0];
  if (v->type == XB_T_STRING) {
    xb_str *r = xb_str_alloc(v->v.s->len);
    for (size_t i = 0; i < r->len; ++i) r->data[i] = (char)(unsigned char)(256 - (unsigned char)v->v.s->data[i]);
    xb_set_str(ret, r);
  } else if (v->type == XB_T_DATE) {
    xb_set_int(ret, 5231808 - v->v.i); /* Clipper's constant: a day number counted down */
  } else if (xb_is_number(v)) {
    xb_set_double(ret, -xb_num_value(v), xb_decimals(ctx));
  } else if (v->type == XB_T_LOGICAL) {
    xb_set_logical(ret, !v->v.l);
  }
}

/* The first `n` bytes of a string argument, zeros past its end. */
static void binary_arg(uint32_t argc, const xb_item *argv, unsigned char *out, size_t n) {
  memset(out, 0, n);
  if (argc && argv[0].type == XB_T_STRING)
    memcpy(out, argv[0].v.s->data, argv[0].v.s->len < n ? argv[0].v.s->len : n);
}

/* Bin2I( c ), Bin2W( c ), Bin2L( c ): the number in a string's first bytes,
 * little-endian: a signed 16-bit, an unsigned 16-bit, a signed 32-bit one. */
void xbf_BIN2I(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  unsigned char b[2];
  binary_arg(argc, argv, b, 2);
  xb_set_int(ret, (int16_t)xb_get_le16(b));
}
void xbf_BIN2W(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  unsigned char b[2];
  binary_arg(argc, argv, b, 2);
  xb_set_int(ret, xb_get_le16(b));
}
void xbf_BIN2L(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  unsigned char b[4];
  binary_arg(argc, argv, b, 4);
  xb_set_int(ret, (int32_t)xb_get_le32(b));
}

/* I2Bin( n ), L2Bin( n ): the number (truncated, its low 16 or 32 bits) as
 * two or four little-endian bytes; anything but a number is 0. */
void xbf_I2BIN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  unsigned char b[2];
  xb_put_le16(b, (uint16_t)(argc && xb_is_number(&argv[0]) ? (uint64_t)xb_num_to_i64(&argv[0]) : 0));
  xb_set_str(ret, xb_str_new((const char *)b, 2));
}
void xbf_L2BIN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  unsigned char b[4];
  xb_put_le32(b, (uint32_t)(argc && xb_is_number(&argv[0]) ? (uint64_t)xb_num_to_i64(&argv[0]) : 0));
  xb_set_str(ret, xb_str_new((const char *)b, 4));
}
