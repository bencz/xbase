/* Number to text, Clipper style (and text to number, Val()).
 *
 * The field has `width` columns for the sign and integer digits, then a dot
 * and `dec` decimals. Doubles keep 16 significant digits (later digits print
 * as 0) and are then rounded half-up to `dec` decimals (clipper_digits). A
 * minus sign appears only when a non-zero digit is shown. A value that does
 * not fit, and any non-finite value, fills the whole field with '*'. */
#include "xbrt_int.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define XB_NUM_PRECISION 16
#define XB_NUM_DIGITS_LOCAL 64 /* digits a double's text is built in on the stack; more are allocated */

/* The text of a field: `width` columns, then a dot and `dec` decimals when dec > 0. */
static size_t field_len(int width, int dec) { return (size_t)width + (dec > 0 ? (size_t)dec + 1 : 0); }

static void overflow(char *s, size_t len) { memset(s, '*', len); }

/* Writes [sign][int digits] right-aligned in the first `width` columns of `s` (blanks before them), then `.frac`
 * (zeros when `frac` is null) when dec > 0. Returns 0 when the integer part does not fit. */
static int place(char *s, int width, int dec, int neg, const char *intDigits, size_t intLen, const char *frac) {
  if (intLen + (neg ? 1u : 0u) > (size_t)width) return 0;
  memset(s, ' ', (size_t)width);
  char *p = s + (size_t)width - intLen;
  memcpy(p, intDigits, intLen);
  if (neg) p[-1] = '-';
  if (dec > 0) {
    s[width] = '.';
    if (frac)
      memcpy(s + width + 1, frac, (size_t)dec);
    else
      memset(s + width + 1, '0', (size_t)dec);
  }
  return 1;
}

#define XB_UINT64_DIGITS 20

/* The decimal digits of `w` (no leading zeros; "0" for 0), written to end at `end`, two at a time: their count. */
static size_t uint_digits(uint64_t w, char *end) {
  static const char pairs[] = "00010203040506070809101112131415161718192021222324252627282930313233343536373839"
                              "40414243444546474849505152535455565758596061626364656667686970717273747576777879"
                              "8081828384858687888990919293949596979899";
  char *p = end;
  for (; w >= 100; w /= 100) {
    p -= 2;
    memcpy(p, pairs + 2 * (w % 100), 2);
  }
  if (w >= 10) {
    p -= 2;
    memcpy(p, pairs + 2 * w, 2);
  } else {
    *--p = (char)('0' + w);
  }
  return (size_t)(end - p);
}

static void format_int(char *s, int64_t v, int width, int dec) {
  char digits[XB_UINT64_DIGITS];
  uint64_t mag = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
  size_t n = uint_digits(mag, digits + sizeof digits);
  if (!place(s, width, dec, v < 0, digits + sizeof digits - n, n, NULL)) overflow(s, field_len(width, dec));
}

/* The half is added to ten times the scaled value and the sum divided back, which is how Clipper gets its results in
 * the last bit, then raised by about one unit in the last place (times 1 + DBL_EPSILON) before truncating, so that a
 * decimal half stored just below it, like 2.675 or 8904.185, still rounds up; a value whose scaled magnitude reaches
 * 2^52 is already its own nearest rounding. */
double xb_num_round(double v, int64_t d) {
  double p = pow(10.0, d < 0 ? -(double)d : (double)d);
  if (v == 0.0 || (d < 0 && xb_dbl_infinite(p))) return 0.0;
  if (d >= 0 && fabs(v) * p >= XB_DBL_INTEGRAL) return v; /* the doubles near v are a step apart or more */
  double tenfold = (d >= 0 ? v * p : v / p) * 10.0;
  tenfold += tenfold < 0 ? -5.0 : 5.0;
  double back = tenfold / 10.0;
  /* back * (1 + DBL_EPSILON) rounded once (fma): a unit that computes with more bits (x87) would round that product
   * twice, and it often falls on the midpoint between two doubles there */
  double r = trunc(fma(back, DBL_EPSILON, back));
  return d >= 0 ? r / p : r * p; /* not finite when the scaling overflows, like Clipper */
}

/* A fraction f in [0, 1), exact: m / 2^s held in 32-bit limbs (s is at most 1126: a subnormal's fraction). Its decimal
 * digits come out in order (fraction_digits): times 10^c, the bits from s up are the next c digits, at most 9 at a
 * time (10^9 times a limb, plus the carry, fits 64 bits; the digits fit the 30 bits above s, in the limb at `top` and
 * the next). */
#define XB_FRACTION_LIMBS 37
#define XB_FRACTION_CHUNK 9
typedef struct fraction {
  uint32_t limb[XB_FRACTION_LIMBS];
  int top, bit, n;
} fraction;

static void fraction_init(fraction *fr, double f) {
  int s = 0;
  uint64_t m = 0;
  if (f > 0.0) {
    int ex;
    m = (uint64_t)ldexp(frexp(f, &ex), XB_DBL_EXACT_BITS);
    s = XB_DBL_EXACT_BITS - ex;
    for (int step = 32; step > 0; step /= 2) /* its trailing zero bits dropped, half the rest at a time */
      if (!(m & (((uint64_t)1 << step) - 1))) {
        m >>= step;
        s -= step;
      }
  }
  fr->top = s / 32;
  fr->bit = s % 32;
  fr->n = fr->top + 2;
  memset(fr->limb, 0, (size_t)fr->n * sizeof fr->limb[0]);
  fr->limb[0] = (uint32_t)m;
  fr->limb[1] = (uint32_t)(m >> 32);
}

/* The fraction's next `count` decimal digits. */
static void fraction_digits(fraction *fr, char *out, int count) {
  static const uint32_t tens[XB_FRACTION_CHUNK + 1] = {1,      10,      100,      1000,      10000,
                                                       100000, 1000000, 10000000, 100000000, 1000000000};
  for (int k = 0; k < count;) {
    int c = count - k < XB_FRACTION_CHUNK ? count - k : XB_FRACTION_CHUNK;
    uint64_t carry = 0;
    for (int i = 0; i < fr->n; ++i) {
      uint64_t p = (uint64_t)fr->limb[i] * tens[c] + carry;
      fr->limb[i] = (uint32_t)p;
      carry = p >> 32;
    }
    uint64_t high = fr->limb[fr->top] | (uint64_t)fr->limb[fr->top + 1] << 32;
    uint32_t chunk = (uint32_t)(high >> fr->bit);
    for (int d = c; d-- > 0; chunk /= 10) out[k + d] = (char)('0' + chunk % 10);
    k += c;
    fr->limb[fr->top] &= (uint32_t)(((uint64_t)1 << fr->bit) - 1);
    fr->limb[fr->top + 1] = 0;
  }
}

/* The digits Clipper shows for |v| with `dec` decimals, when they hold at most XB_NUM_PRECISION significant digits,
 * from the exact digits of the integer part and of the fraction: rounded half up on the digit after the last one
 * shown, a 4 counting as a 5 when the digits after it up to the 16th significant one are 9s and the 17th is not 0 or
 * 1, so that a decimal half stored just below it (8904.185 is 8904.18499999999949...) rounds up, as Clipper shows it.
 * (Clipper takes the fraction's digits from repeated products by ten in doubles, whose error reaches the 13th
 * decimal and beyond: there xbase's are the exact ones.) Writes the integer digits (no leading zeros but one) to
 * `intDigits` and `dec` decimals to `frac`; 0, nothing written, when more significant digits would show (the
 * caller's way, on 16 correctly rounded ones, takes those). */
static int clipper_digits(double a, int dec, char *intDigits, size_t *intLen, char *frac) {
  double whole, f = modf(a, &whole);
  if (!(whole < (double)UINT64_MAX) || dec > XB_NUM_PRECISION) return 0;
  char digitsOf[XB_UINT64_DIGITS];
  int len = (int)uint_digits((uint64_t)whole, digitsOf + sizeof digitsOf);
  if (len > XB_NUM_PRECISION) return 0;
  const char *ints = digitsOf + sizeof digitsOf - len;
  /* The fraction's digits: the `dec` shown and the next one, then, after a 4, those the precision leaves. */
  char digits[2 * XB_NUM_PRECISION + 1];
  fraction fr;
  fraction_init(&fr, f);
  fraction_digits(&fr, digits, dec + 1);
  int sig = whole >= 1.0 ? len : 0;
  for (int k = 0; k < dec; ++k) sig += sig > 0 || digits[k] != '0';
  if (sig > XB_NUM_PRECISION) return 0;
  int room = sig > 0 ? XB_NUM_PRECISION - sig : 0, next = digits[dec] - '0';
  if (next == 4 && room > 0) {
    fraction_digits(&fr, digits + dec + 1, room);
    int t = 1;
    for (; t <= room; ++t) {
      int d = digits[dec + t] - '0';
      if (d < 9 && (t != room || d < 2)) break;
    }
    if (t > room) next = 5;
  }
  /* The integer digits and the decimals, rounded up from the end when `next` is 5 or more. */
  memcpy(intDigits + 1, ints, (size_t)len);
  intDigits[0] = '0';
  memcpy(frac, digits, (size_t)dec);
  int carry = next >= 5;
  for (int k = dec; carry && k-- > 0;) {
    carry = frac[k] == '9';
    frac[k] = carry ? '0' : (char)(frac[k] + 1);
  }
  for (int k = len + 1; carry && k-- > 0;) {
    carry = intDigits[k] == '9';
    intDigits[k] = carry ? '0' : (char)(intDigits[k] + 1);
  }
  size_t first = 0;
  while (first + 1 < (size_t)len + 1 && intDigits[first] == '0') ++first;
  memmove(intDigits, intDigits + first, (size_t)len + 1 - first);
  *intLen = (size_t)len + 1 - first;
  return 1;
}

/* The decimal digits of a double of 2^64 or more, an integer: its significand shifted into 32-bit limbs (2^1024 needs
 * 32), then divided by 10^9 again and again, each remainder nine digits from the right. Written to end at `end`
 * (XB_BIG_DIGITS bytes before it are room enough: 2^1024 has 309 digits); their count. */
#define XB_BIG_LIMBS 34
#define XB_BIG_DIGITS 320
static size_t big_digits(double a, char *end) {
  uint32_t limb[XB_BIG_LIMBS] = {0};
  int ex;
  uint64_t m = (uint64_t)ldexp(frexp(a, &ex), XB_DBL_EXACT_BITS); /* a = m * 2^(ex - 53), ex - 53 > 0 here */
  int shift = ex - XB_DBL_EXACT_BITS, at = shift / 32, bit = shift % 32;
  uint64_t low = m << bit, high = bit ? m >> (64 - bit) : 0; /* m << bit across 96 bits */
  limb[at] = (uint32_t)low;
  limb[at + 1] = (uint32_t)(low >> 32);
  limb[at + 2] = (uint32_t)high;
  int n = at + 3;
  char *p = end;
  while (n > 0) {
    uint64_t rem = 0;
    for (int i = n; i-- > 0;) {
      uint64_t cur = rem << 32 | limb[i];
      limb[i] = (uint32_t)(cur / 1000000000u);
      rem = cur % 1000000000u;
    }
    while (n > 0 && limb[n - 1] == 0) --n;
    for (int k = 0; k < 9 && (n > 0 || rem > 0); ++k, rem /= 10) *--p = (char)('0' + rem % 10);
  }
  return (size_t)(end - p);
}

/* The first XB_NUM_PRECISION significant digits of a (finite, above 0) into m, rounded half up on the next one as
 * Clipper rounds them (1286742750677284.5 is ...285), from the exact digits: an integer part below 2^64 and the
 * fraction's (fraction_digits), or those of a larger double, an integer (big_digits). Returns the decimal exponent of
 * m[0]. */
static int significant_digits(double a, char m[XB_NUM_PRECISION]) {
  char d[XB_NUM_PRECISION + 1]; /* one digit more: the one that rounds */
  int n = 0, e;
  if (a < 18446744073709551616.0) { /* 2^64 */
    double whole, f = modf(a, &whole);
    fraction fr;
    fraction_init(&fr, f);
    if (whole >= 1.0) {
      char ints[XB_UINT64_DIGITS];
      size_t len = uint_digits((uint64_t)whole, ints + sizeof ints);
      e = (int)len - 1;
      for (; n <= XB_NUM_PRECISION && (size_t)n < len; ++n) d[n] = ints[sizeof ints - len + (size_t)n];
    } else {
      for (e = -1;; --e) { /* the fraction's leading zeros */
        fraction_digits(&fr, d, 1);
        if (d[0] != '0') break;
      }
      n = 1;
    }
    if (n <= XB_NUM_PRECISION) fraction_digits(&fr, d + n, XB_NUM_PRECISION + 1 - n);
  } else {
    char ints[XB_BIG_DIGITS];
    size_t len = big_digits(a, ints + sizeof ints);
    e = (int)len - 1;
    memcpy(d, ints + sizeof ints - len, XB_NUM_PRECISION + 1);
  }
  memcpy(m, d, XB_NUM_PRECISION);
  if (d[XB_NUM_PRECISION] >= '5') {
    int k = XB_NUM_PRECISION;
    while (k-- > 0 && m[k] == '9') m[k] = '0';
    if (k >= 0) {
      ++m[k];
    } else { /* 9999999999999999.5: 1 and zeros, one place up */
      m[0] = '1';
      ++e;
    }
  }
  return e;
}

static void format_double(char *s, double v, int width, int dec) {
  if (!xb_dbl_finite(v)) {
    overflow(s, field_len(width, dec));
    return;
  }
  if (fabs(v) < XB_DBL_INTEGRAL && v == trunc(v)) { /* an integer's digits and zero decimals, as below; -0.0 unsigned */
    format_int(s, (int64_t)v, width, dec);
    return;
  }
  char intDigits[XB_NUM_PRECISION + 2], frac[XB_NUM_PRECISION];
  size_t intLen;
  if (clipper_digits(fabs(v), dec, intDigits, &intLen, frac)) {
    int nonZero = !(intLen == 1 && intDigits[0] == '0');
    for (int k = 0; k < dec; ++k) nonZero |= frac[k] != '0';
    if (!place(s, width, dec, v < 0 && nonZero, intDigits, intLen, frac)) overflow(s, field_len(width, dec));
    return;
  }

  /* Significant digits m[0..15] of |v|, where m[k] has decimal exponent e - k. */
  char m[XB_NUM_PRECISION];
  int e = significant_digits(fabs(v), m);

  /* digits[i] holds the digit of exponent hi - i, from hi down to -dec
   * (positions in 64 bits: dec may be near INT_MAX). */
  int hi = (e > 0 ? e : 0) + 1; /* one spare column for a rounding carry */
  size_t n = (size_t)hi + (size_t)dec + 1;
  char local[XB_NUM_DIGITS_LOCAL];
  char *digits = n <= sizeof local ? local : malloc(n);
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
  if (digits != local) free(digits);
  if (!ok) overflow(s, field_len(width, dec));
}

static void format(char *s, const xb_item *it, int width, int dec) {
  if (it->type == XB_T_INT) format_int(s, it->v.i, width, dec);
  else format_double(s, it->v.d, width, dec);
}

size_t xb_num_text_len(int width, int dec) { return field_len(width, dec < 0 ? 0 : dec); }

void xb_num_text(const xb_item *it, int width, int dec, char *out) { format(out, it, width, dec < 0 ? 0 : dec); }

xb_str *xb_num_to_str(const xb_item *it, int width, int dec) {
  xb_str *s = xb_str_alloc(xb_num_text_len(width, dec));
  xb_num_text(it, width, dec, xb_str_buffer(s));
  return s;
}

void xb_num_to_field_buf(const xb_item *it, int width, int dec, char *out) {
  if (dec > 0 && dec >= width) {
    memset(out, '*', (size_t)width);
    return;
  }
  format(out, it, dec > 0 ? width - dec - 1 : width, dec < 0 ? 0 : dec);
}

void xb_num_parse(const char *s, size_t len, xb_item *r) {
  size_t pos = 0;
  while (pos < len && xb_is_blank(s[pos])) ++pos;
  int neg = 0;
  if (pos < len && (s[pos] == '-' || s[pos] == '+')) neg = s[pos++] == '-';
  size_t width = pos, dec = 0, decRest = 0;
  int64_t iv = 0;
  /* The digits read and the point, for the double's correctly rounded value (strtod, as the compiler's and the macro
   * compiler's literals: the runtime never changes the C locale). */
  char small[64];
  char *text = len + 2 <= sizeof small ? small : malloc(len + 2);
  if (!text) xb_gc_fatal("out of memory");
  size_t n = 0;
  int isDouble = 0, inDec = 0;
  for (; pos < len; ++pos) {
    char c = s[pos];
    if (c >= '0' && c <= '9') {
      if (!isDouble && (xb_mul_overflow(iv, 10, &iv) || xb_add_overflow(iv, c - '0', &iv))) isDouble = 1;
      text[n++] = c;
      if (inDec) ++dec;
      else ++width;
    } else if (c == '.' && !inDec) {
      inDec = 1;
      isDouble = 1;
      text[n++] = '.';
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
  text[n] = '\0';
  double dv = isDouble ? strtod(text, NULL) : (double)iv;
  if (text != small) free(text);
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
