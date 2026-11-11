/* Number functions and number/text conversion. */
#include "xbrt_int.h"

#include <float.h>
#include <math.h>
#include <string.h>

#define XB_STR_MAX_WIDTH 90 /* widest field Str() takes from an item */

/* Default field of a number: its own width, or the default for its value. */
static void item_field(const xb_item *it, int *width, int *dec) {
  if (it->type == XB_T_INT) {
    *width = it->width ? it->width : xb_num_int_width(it->v.i);
    *dec = 0;
  } else {
    *width = it->width ? it->width : xb_num_dbl_width(it->v.d);
    *dec = it->dec;
  }
}

/* Every argument passed, even NIL, must be a number. */
static int str_args_ok(uint32_t argc, const xb_item *argv) {
  for (uint32_t i = 0; i < argc && i < 3; ++i)
    if (!xb_is_number(&argv[i])) return 0;
  return argc >= 1;
}

/* The text of Str( n [, nWidth [, nDec]] ). A field too narrow for its
 * decimals is all stars. */
static xb_str *str_of(uint32_t argc, const xb_item *argv) {
  int width, dec;
  item_field(&argv[0], &width, &dec);
  if (width > XB_STR_MAX_WIDTH) width = XB_STR_MAX_WIDTH;
  int hasWidth = argc > 1;
  if (hasWidth) {
    int64_t w = xb_num_to_i64(&argv[1]);
    width = w < 1 ? xb_num_int_width(0) : (w > INT32_MAX ? INT32_MAX : (int)w);
    dec = 0;
  }
  if (width > 1 && argc > 2) {
    int64_t d = xb_num_to_i64(&argv[2]);
    dec = d <= 0 ? 0 : (d > INT32_MAX ? INT32_MAX : (int)d);
    if (hasWidth && dec > 0) {
      if (dec >= width) {
        xb_str *s = xb_str_alloc((size_t)width);
        memset(s->data, '*', (size_t)width);
        return s;
      }
      width -= dec + 1;
    }
  }
  return xb_num_to_str(&argv[0], width, dec);
}

/* Str( n [, nWidth [, nDec]] ): nWidth is the whole field (with the dot and
 * the decimals); without it, the number's own field is used. */
void xbf_STR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (!str_args_ok(argc, argv)) {
    xb_fn_arg_error(ctx, ret, 1099, "STR", argc, argv);
    return;
  }
  xb_set_str(ret, str_of(argc, argv));
}

/* StrZero(): Str() with the leading blanks (after a sign) turned into zeros. */
void xbf_STRZERO(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (!str_args_ok(argc, argv)) {
    xb_fn_arg_error(ctx, ret, 6003, "STRZERO", argc, argv);
    return;
  }
  xb_str *s = str_of(argc, argv);
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
  const char *s = argv[0].v.s->data;
  size_t len = argv[0].v.s->len, pos = 0;
  while (pos < len && xb_is_blank(s[pos])) ++pos;
  int neg = 0;
  if (pos < len && (s[pos] == '-' || s[pos] == '+')) neg = s[pos++] == '-';
  size_t width = pos, dec = 0, decRest = 0;
  int64_t iv = 0;
  double dv = 0.0;
  int isDouble = 0, inDec = 0;
  for (; pos < len; ++pos) {
    char c = s[pos];
    if (c >= '0' && c <= '9') {
      if (!isDouble && (xb_mul_overflow(iv, 10, &iv) || xb_add_overflow(iv, c - '0', &iv))) isDouble = 1;
      dv = dv * 10.0 + (c - '0');
      if (inDec) ++dec;
      else ++width;
    } else if (c == '.' && !inDec) {
      inDec = 1;
      isDouble = 1;
    } else {
      while (!inDec && pos < len) { /* characters up to a dot widen the field */
        if (s[pos++] == '.') inDec = 1;
        else ++width;
      }
      if (inDec) { /* a dot after other characters still makes a number with decimals */
        decRest = len - pos;
        isDouble = 1;
      }
      break;
    }
  }
  if (dec) dv /= pow(10.0, (double)dec);
  dec += decRest;
  double d = neg ? -dv : dv;
  int64_t i = neg ? (int64_t)(0 - (uint64_t)iv) : iv; /* iv may have wrapped (a double then) */
  uint16_t w;
  if (width > 10) w = isDouble ? xb_num_dbl_width(d) : xb_num_int_width(i);
  else if (dec == 0) w = (uint16_t)len;
  else if (width == 0) w = 1;
  else if (neg && width == 1 && dv != 0) w = 2;
  else w = (uint16_t)width;
  uint64_t value = (uint64_t)i;
  if (isDouble) memcpy(&value, &d, 8);
  uint16_t dec16 = isDouble ? (uint16_t)(dec > UINT16_MAX ? UINT16_MAX : dec) : 0;
  xb_put(ret, xb_header_word(isDouble ? XB_T_DOUBLE : XB_T_INT, w, dec16), value);
}

void xbf_INT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, 1090, "INT", argc, argv);
    return;
  }
  if (argv[0].type == XB_T_INT) {
    xb_set_int(ret, argv[0].v.i);
    return;
  }
  double t = trunc(argv[0].v.d);
  if (xb_dbl_fits_i64(t)) xb_set_int(ret, (int64_t)t);
  else xb_set_double(ret, t, 0);
}

void xbf_ABS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) {
    xb_fn_arg_error(ctx, ret, 1089, "ABS", argc, argv);
    return;
  }
  if (argv[0].type == XB_T_INT && argv[0].v.i != INT64_MIN)
    xb_set_int(ret, argv[0].v.i < 0 ? -argv[0].v.i : argv[0].v.i);
  else xb_set_double(ret, fabs(xb_num_value(&argv[0])), argv[0].type == XB_T_DOUBLE ? argv[0].dec : 0);
}

/* Round( n, nDec ): half away from zero; a negative nDec rounds to tens,
 * hundreds, ... The half is added to ten times the scaled value and the sum
 * divided back, which is how Clipper gets its results in the last bit, then
 * raised by about one unit in the last place (times 1 + DBL_EPSILON) before
 * truncating, so that a decimal half stored just below it, like 2.675, still
 * rounds up. Int() truncates the value as it is. */
void xbf_ROUND(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 2 || !xb_is_number(&argv[0]) || !xb_is_number(&argv[1])) {
    xb_fn_arg_error(ctx, ret, 1094, "ROUND", argc, argv);
    return;
  }
  int64_t d = xb_num_to_i64(&argv[1]);
  double v = xb_num_value(&argv[0]);
  double p = pow(10.0, d < 0 ? -(double)d : (double)d);
  double r;
  if (v == 0.0 || (d < 0 && xb_dbl_infinite(p))) {
    r = 0.0;
  } else {
    double tenfold = (d >= 0 ? v * p : v / p) * 10.0;
    tenfold += tenfold < 0 ? -5.0 : 5.0;
    double back = tenfold / 10.0;
    /* back * (1 + DBL_EPSILON) rounded once (fma): a unit that computes with
     * more bits (x87) would round that product twice, and it often falls on
     * the midpoint between two doubles there */
    r = trunc(fma(back, DBL_EPSILON, back));
    r = d >= 0 ? r / p : r * p; /* not finite when the scaling overflows, like Clipper */
  }
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
static double math_sqrt(double x) { return x < 0 ? 0.0 : sqrt(x); }
static double math_exp(double x) {
  double r = exp(x);
  return xb_dbl_infinite(r) ? 0.0 : r;
}
static double math_log(double x) { return x < 0 ? -HUGE_VAL : log(x); }

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
