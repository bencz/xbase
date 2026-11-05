/* Date functions of the language. */
#include "xbrt_int.h"

#include <string.h>

static int64_t date_arg(xb_ctx *ctx, uint32_t argc, const xb_item *argv, const char *fn, int code, int *ok) {
  if (argc >= 1 && argv[0].type == XB_T_DATE) {
    *ok = 1;
    return argv[0].v.i;
  }
  *ok = 0;
  xb_rt_error(ctx, "BASE", code, "Argument error", fn);
  return 0;
}

/* Number results of the date functions have fixed display widths. */
static void ret_number(xb_item *ret, int64_t v, uint16_t width) {
  xb_set_int(ret, v);
  ret->width = width;
}

void xbf_DATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_date(ret, xb_date_today());
}

void xbf_CTOD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || argv[0].type != XB_T_STRING) {
    xb_rt_error(ctx, "BASE", 1119, "Argument error", "CTOD");
    return;
  }
  xb_set_date(ret, xb_date_parse(argv[0].v.s->data, argv[0].v.s->len, ctx->date_format, ctx->epoch));
}

void xbf_DTOC(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int ok;
  int64_t jd = date_arg(ctx, argc, argv, "DTOC", 1118, &ok);
  if (!ok) return;
  char out[XB_DATE_TEXT_SIZE];
  size_t n = xb_date_format(jd, ctx->date_format, out);
  xb_set_str(ret, xb_str_new(out, n));
}

void xbf_DTOS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int ok;
  int64_t jd = date_arg(ctx, argc, argv, "DTOS", 1120, &ok);
  if (!ok) return;
  char out[XB_DATE_DIGITS_SIZE];
  xb_date_to_digits(jd, out);
  xb_set_str(ret, xb_str_new(out, XB_DATE_DIGITS_SIZE - 1));
}

/* SToD( "YYYYMMDD" ): anything else gives the empty date. */
void xbf_STOD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  int64_t jd = 0;
  if (argc >= 1 && argv[0].type == XB_T_STRING && argv[0].v.s->len >= 8) {
    const char *s = argv[0].v.s->data;
    int v[8], ok = 1;
    for (int i = 0; i < 8; ++i) {
      ok &= s[i] >= '0' && s[i] <= '9';
      v[i] = s[i] - '0';
    }
    if (ok) jd = xb_date_encode(v[0] * 1000 + v[1] * 100 + v[2] * 10 + v[3], v[4] * 10 + v[5], v[6] * 10 + v[7]);
  }
  xb_set_date(ret, jd);
}

static void date_part(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, const char *fn, int part) {
  static const int codes[] = {1112, 1113, 1114}; /* YEAR, MONTH, DAY */
  int ok;
  int64_t jd = date_arg(ctx, argc, argv, fn, codes[part], &ok);
  if (!ok) return;
  int y, m, d;
  xb_date_decode(jd, &y, &m, &d);
  if (part == 0) ret_number(ret, y, 5);
  else if (part == 1) ret_number(ret, m, 3);
  else ret_number(ret, d, 3);
}

void xbf_YEAR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { date_part(ctx, ret, argc, argv, "YEAR", 0); }
void xbf_MONTH(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { date_part(ctx, ret, argc, argv, "MONTH", 1); }
void xbf_DAY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { date_part(ctx, ret, argc, argv, "DAY", 2); }

/* 1 = Sunday; 0 for a day number outside the calendar (and the empty date). */
static int day_of_week(int64_t jd) {
  int y, m, d;
  xb_date_decode(jd, &y, &m, &d);
  return m ? (int)((jd + 1) % 7) + 1 : 0;
}

void xbf_DOW(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int ok;
  int64_t jd = date_arg(ctx, argc, argv, "DOW", 1115, &ok);
  if (ok) ret_number(ret, day_of_week(jd), 3);
}

/* CDoW() takes the weekday from the decoded year, month and day with the
 * Gregorian weekday formula, as Clipper does, so it can differ from DoW() in
 * year 0 and for day numbers outside the calendar (decoded as 0/0/0). In the
 * formula January and February are months 13 and 14 of the previous year and
 * the other months count one more; month 0 counts as 13. Divisions truncate;
 * 1 = Sunday. */
static int weekday_of_fields(int64_t jd) {
  int y, m, d;
  xb_date_decode(jd, &y, &m, &d);
  int fm = m < 3 ? m + 13 : m + 1;
  int fy = m < 3 ? y - 1 : y;
  return (d + 26 * fm / 10 + fy + fy / 4 - fy / 100 + fy / 400 + 6) % 7 + 1;
}

void xbf_CDOW(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  static const char *const names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  int ok;
  int64_t jd = date_arg(ctx, argc, argv, "CDOW", 1117, &ok);
  if (!ok) return;
  const char *s = jd ? names[weekday_of_fields(jd) - 1] : "";
  xb_set_str(ret, xb_str_new(s, strlen(s)));
}

void xbf_CMONTH(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  static const char *const names[] = {"January", "February", "March",     "April",   "May",      "June",
                                      "July",    "August",   "September", "October", "November", "December"};
  int ok;
  int64_t jd = date_arg(ctx, argc, argv, "CMONTH", 1116, &ok);
  if (!ok) return;
  int y, m, d;
  xb_date_decode(jd, &y, &m, &d);
  const char *s = m ? names[m - 1] : "";
  xb_set_str(ret, xb_str_new(s, strlen(s)));
}
