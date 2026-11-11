/* Dates. A date item holds a Julian day number (0: the empty date). Dates are
 * built for years 0 to 9999; arithmetic can go past 9999, and those day
 * numbers still decode, up to XB_DATE_DECODE_LAST. Day numbers below year 0
 * or above that limit have no calendar fields. Conversions use the
 * Fliegel-Van Flandern formulas. */
#include "xbrt_int.h"

#include <string.h>

#define XB_DATE_FIRST 1721060 /* day number of 0000-01-01 */
#define XB_DATE_DECODE_LAST INT32_MAX /* keeps the formulas far from overflowing */

int64_t xb_date_encode(int year, int month, int day) {
  static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (year < 0 || year > 9999 || month < 1 || month > 12 || day < 1) return 0;
  int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  if (day > days[month - 1] && !(month == 2 && day == 29 && leap)) return 0;
  int64_t a = month < 3 ? -1 : 0;
  return (a + 4800 + year) * 1461 / 4 + (month - 2 - a * 12) * 367 / 12 - (a + 4900 + year) / 100 * 3 / 4 + day -
         32075;
}

void xb_date_decode(int64_t jd, int *year, int *month, int *day) {
  if (jd < XB_DATE_FIRST || jd > XB_DATE_DECODE_LAST) {
    *year = *month = *day = 0;
    return;
  }
  int64_t l = jd + 68569;
  int64_t n = 4 * l / 146097;
  l -= (146097 * n + 3) / 4;
  int64_t i = 4000 * (l + 1) / 1461001;
  l = l - 1461 * i / 4 + 31;
  int64_t j = 80 * l / 2447;
  *day = (int)(l - 2447 * j / 80);
  l = j / 11;
  *month = (int)(j + 2 - 12 * l);
  *year = (int)(100 * (n - 49) + i + l);
}

void xb_set_date(xb_item *it, int64_t jd) { xb_put(it, xb_header_word(XB_T_DATE, 0, 0), (uint64_t)jd); }

/* "YYYYMMDD" with the last four digits of the year: spaces for the empty
 * date and negative day numbers, zeros for other day numbers outside the
 * calendar. */
void xb_date_to_digits(int64_t jd, char out[XB_DATE_DIGITS_SIZE]) {
  if (jd <= 0) {
    memset(out, ' ', XB_DATE_DIGITS_SIZE - 1);
    out[XB_DATE_DIGITS_SIZE - 1] = '\0';
    return;
  }
  int y, m, d;
  xb_date_decode(jd, &y, &m, &d);
  y %= 10000;
  for (int k = 3; k >= 0; --k, y /= 10) out[k] = (char)('0' + y % 10);
  out[4] = (char)('0' + m / 10);
  out[5] = (char)('0' + m % 10);
  out[6] = (char)('0' + d / 10);
  out[7] = (char)('0' + d % 10);
  out[8] = '\0';
}

/* Date text by a SET DATE FORMAT picture, Clipper's rules. The picture is
 * read in runs of the same character (letters compared in upper case) and
 * written in upper case, at most 10 characters. The first run of D, M or Y
 * becomes digits of its field: with tens t and units u, a run of 4 gives
 * t t t u, 3 gives t t u, 2 gives t u, and any other length u followed by the
 * remaining letters; a year run of 4, 3 or 2 gives its last digits, other
 * lengths the last digit and the remaining letters. Later runs of the same
 * field, and other characters, are copied. The empty date has blank digits. */
size_t xb_date_format(int64_t jd, const char *fmt, char out[XB_DATE_TEXT_SIZE]) {
  char digits[XB_DATE_DIGITS_SIZE];
  xb_date_to_digits(jd, digits);
  return xb_date_format_digits(digits, fmt, out);
}

size_t xb_date_format_digits(const char *digits, const char *fmt, char out[XB_DATE_TEXT_SIZE]) {
  size_t size = strlen(fmt);
  if (size > XB_DATE_TEXT_SIZE - 1) size = XB_DATE_TEXT_SIZE - 1;
  size_t n = 0;
  int usedD = 0, usedM = 0, usedY = 0;
  for (size_t i = 0; i < size;) {
    char up = xb_ascii_upper(fmt[i]);
    size_t run = 1;
    while (i + run < size && xb_ascii_upper(fmt[i + run]) == up) ++run;
    i += run;
    int *used = up == 'D' ? &usedD : up == 'M' ? &usedM : up == 'Y' ? &usedY : NULL;
    size_t written = 0;
    if (used && !*used) {
      if (up == 'Y') {
        size_t take = run >= 2 && run <= 4 ? run : 1;
        for (size_t k = 4 - take; k < 4; ++k) out[n++] = digits[k];
        written = take;
      } else {
        const char *f = digits + (up == 'D' ? 6 : 4); /* tens, units */
        size_t tens = run >= 2 && run <= 4 ? run - 1 : 0;
        for (size_t k = 0; k < tens; ++k) out[n++] = f[0];
        out[n++] = f[1];
        written = tens + 1;
      }
      *used = 1;
    }
    for (; written < run; ++written) out[n++] = up;
  }
  out[n] = '\0';
  return n;
}

/* Two-digit years fall in the hundred years starting at SET EPOCH. */
int xb_date_epoch_year(int year, int epoch) {
  if (year < 0 || year >= 100) return year;
  int century = epoch / 100;
  if (year < epoch % 100) ++century;
  return year + century * 100;
}

/* Text to date by a SET DATE FORMAT picture: the order of the first D, M and
 * Y in the picture gives the order of the fields; each run of digits fills
 * the current field and the first non-digit after it moves to the next one.
 * Anything invalid gives the empty date. */
int64_t xb_date_parse(const char *text, size_t len, const char *fmt, int epoch) {
  int order[3] = {0, 0, 0}; /* 'D', 'M' or 'Y' by field position */
  int fields = 0;
  for (const char *p = fmt; *p && fields < 3; ++p) {
    char up = xb_ascii_upper(*p);
    if (up != 'D' && up != 'M' && up != 'Y') continue;
    int seen = 0;
    for (int k = 0; k < fields; ++k) seen |= order[k] == up;
    if (!seen) order[fields++] = up;
  }
  int value[3] = {0, 0, 0};
  int field = 0, inDigits = 0;
  for (size_t i = 0; i < len && field < 3; ++i) {
    char c = text[i];
    if (c >= '0' && c <= '9') {
      if (value[field] < 100000) value[field] = value[field] * 10 + (c - '0'); /* larger is invalid anyway */
      inDigits = 1;
    } else if (inDigits) {
      inDigits = 0;
      ++field;
    }
  }
  int d = 0, m = 0, y = 0;
  for (int k = 0; k < fields; ++k) {
    if (order[k] == 'D') d = value[k];
    else if (order[k] == 'M') m = value[k];
    else y = value[k];
  }
  return xb_date_encode(xb_date_epoch_year(y, epoch), m, d);
}

int64_t xb_date_today(void) {
  int y, m, d;
  xb_platform_local_date(&y, &m, &d);
  return xb_date_encode(y, m, d);
}
