/* Number to text, Clipper style (and text to number, Val()).
 *
 * The field has `width` columns for the sign and integer digits, then a dot
 * and `dec` decimals. Doubles keep 16 significant digits (later digits print
 * as 0) and are then rounded half-up to `dec` decimals. A minus sign appears
 * only when a non-zero digit is shown. A value that does not fit, and any
 * non-finite value, fills the whole field with '*'. */
#include "xbrt_int.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XB_NUM_PRECISION 16

static xb_str *field(int width, int dec) {
  size_t total = (size_t)width + (dec > 0 ? (size_t)dec + 1 : 0);
  xb_str *s = xb_str_alloc(total);
  memset(s->data, ' ', total);
  return s;
}

static xb_str *overflow(xb_str *s) {
  memset(s->data, '*', s->len);
  return s;
}

/* Writes [sign][int digits] right-aligned in the first `width` columns, then
 * `.frac` (zeros when `frac` is null) when dec > 0. Returns 0 when the integer
 * part does not fit. */
static int place(xb_str *s, int width, int dec, int neg, const char *intDigits, size_t intLen, const char *frac) {
  if (intLen + (neg ? 1u : 0u) > (size_t)width) return 0;
  char *p = s->data + (size_t)width - intLen;
  memcpy(p, intDigits, intLen);
  if (neg) p[-1] = '-';
  if (dec > 0) {
    s->data[width] = '.';
    if (frac)
      memcpy(s->data + width + 1, frac, (size_t)dec);
    else
      memset(s->data + width + 1, '0', (size_t)dec);
  }
  return 1;
}

static xb_str *format_int(int64_t v, int width, int dec) {
  xb_str *s = field(width, dec);
  char digits[24];
  uint64_t mag = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
  size_t n = 0;
  char rev[24];
  do {
    rev[n++] = (char)('0' + (int)(mag % 10u));
    mag /= 10u;
  } while (mag);
  for (size_t i = 0; i < n; ++i) digits[i] = rev[n - 1 - i];
  return place(s, width, dec, v < 0, digits, n, NULL) ? s : overflow(s);
}

static xb_str *format_double(double v, int width, int dec) {
  xb_str *s = field(width, dec);
  if (!xb_dbl_finite(v)) return overflow(s);

  /* Significant digits m[0..15] of |v|, where m[k] has decimal exponent e - k. */
  char sci[48];
  snprintf(sci, sizeof sci, "%.*e", XB_NUM_PRECISION - 1, fabs(v));
  char m[XB_NUM_PRECISION];
  m[0] = sci[0];
  memcpy(m + 1, sci + 2, XB_NUM_PRECISION - 1);
  int e = atoi(strchr(sci, 'e') + 1);

  /* digits[i] holds the digit of exponent hi - i, from hi down to -dec
   * (positions in 64 bits: dec may be near INT_MAX). */
  int hi = (e > 0 ? e : 0) + 1; /* one spare column for a rounding carry */
  size_t n = (size_t)hi + (size_t)dec + 1;
  char *digits = malloc(n);
  if (!digits) xb_gc_fatal("out of memory");
  memset(digits, '0', n);
  int roundUp = 0;
  for (int k = 0; k < XB_NUM_PRECISION; ++k) {
    int exp = e - k;
    if (exp >= -dec) {
      digits[(int64_t)hi - exp] = m[k];
    } else {
      roundUp = exp == -dec - 1 && m[k] >= '5'; /* only the digit right after the last one shown rounds */
      break;
    }
  }
  for (size_t i = n; roundUp && i-- > 0;) {
    if (digits[i] == '9') {
      digits[i] = '0';
    } else {
      digits[i]++;
      roundUp = 0;
    }
  }

  size_t intEnd = (size_t)hi + 1; /* digits[0..intEnd) is the integer part */
  size_t first = 0;
  while (first + 1 < intEnd && digits[first] == '0') ++first;
  int nonZero = 0;
  for (size_t i = 0; i < n; ++i) nonZero |= digits[i] != '0';

  int ok = place(s, width, dec, v < 0 && nonZero, digits + first, intEnd - first, digits + intEnd);
  free(digits);
  return ok ? s : overflow(s);
}

xb_str *xb_num_to_str(const xb_item *it, int width, int dec) {
  if (dec < 0) dec = 0;
  return it->type == XB_T_INT ? format_int(it->v.i, width, dec) : format_double(it->v.d, width, dec);
}

xb_str *xb_num_to_field(const xb_item *it, int width, int dec) {
  if (dec > 0) {
    if (dec >= width) {
      xb_str *s = xb_str_alloc((size_t)width);
      memset(s->data, '*', (size_t)width);
      return s;
    }
    width -= dec + 1;
  }
  return xb_num_to_str(it, width, dec);
}

void xb_num_parse(const char *s, size_t len, xb_item *r) {
  size_t pos = 0;
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
  xb_put(r, xb_header_word(isDouble ? XB_T_DOUBLE : XB_T_INT, w, dec16), value);
}
