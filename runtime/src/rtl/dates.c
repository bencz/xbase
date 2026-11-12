/* Date functions of the language. */
#include "xbrt_int.h"

static int64_t date_arg(xb_ctx *ctx, xb_item *ret, uint32_t argc, const xb_item *argv, const char *fn, int code,
                        int *ok) {
  if (argc >= 1 && argv[0].type == XB_T_DATE) {
    *ok = 1;
    return argv[0].v.i;
  }
  *ok = 0;
  xb_fn_arg_error(ctx, ret, code, fn, argc, argv);
  return 0;
}

/* Number results of the date functions have fixed display widths. */
static void ret_number(xb_item *ret, int64_t v, uint16_t width) {
  xb_put(ret, xb_header_word(XB_T_INT, width, 0), (uint64_t)v);
}

void xbf_DATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_date(ret, xb_date_today());
}

/* Seconds(): seconds since midnight, local time, to the millisecond; SET
 * DECIMALS decimals. */
void xbf_SECONDS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  int h, m, s, ms;
  xb_platform_local_time(&h, &m, &s, &ms);
  xb_set_double(ret, h * 3600.0 + m * 60.0 + s + ms / 1000.0, xb_decimals(ctx));
}

/* Time(): the local time of day as "hh:mm:ss". */
void xbf_TIME(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  int h, m, s, ms;
  xb_platform_local_time(&h, &m, &s, &ms);
  char text[9] = {(char)('0' + h / 10), (char)('0' + h % 10), ':', (char)('0' + m / 10), (char)('0' + m % 10), ':',
                  (char)('0' + s / 10), (char)('0' + s % 10), '\0'};
  xb_set_str(ret, xb_str_new(text, 8));
}

/* ElapsedMs(): milliseconds since the program started, from a monotonic
 * clock: the difference of two calls measures the time between them. */
void xbf_ELAPSEDMS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_int(ret, xb_program_elapsed_ms());
}

void xbf_CTOD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || argv[0].type != XB_T_STRING) {
    xb_fn_arg_error(ctx, ret, 1119, "CTOD", argc, argv);
    return;
  }
  xb_set_date(ret, xb_date_parse(argv[0].v.s->data, argv[0].v.s->len, ctx->date_format, ctx->epoch));
}

void xbf_DTOC(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int ok;
  int64_t jd = date_arg(ctx, ret, argc, argv, "DTOC", 1118, &ok);
  if (!ok) return;
  char out[XB_DATE_TEXT_SIZE];
  size_t n = xb_date_format(jd, ctx->date_format, out);
  xb_set_str(ret, xb_str_new(out, n));
}

void xbf_DTOS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int ok;
  int64_t jd = date_arg(ctx, ret, argc, argv, "DTOS", 1120, &ok);
  if (!ok) return;
  char out[XB_DATE_DIGITS_SIZE];
  xb_date_to_digits(jd, out);
  xb_set_str(ret, xb_str_new(out, XB_DATE_DIGITS_SIZE - 1));
}

/* SToD( "YYYYMMDD" ): anything else gives the empty date. */
void xbf_STOD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  int ok = argc >= 1 && argv[0].type == XB_T_STRING && argv[0].v.s->len >= XB_DATE_DIGITS_SIZE - 1;
  xb_set_date(ret, ok ? xb_date_from_digits(argv[0].v.s->data) : 0);
}

static void date_part(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, const char *fn, int part) {
  static const int codes[] = {1112, 1113, 1114}; /* YEAR, MONTH, DAY */
  int ok;
  int64_t jd = date_arg(ctx, ret, argc, argv, fn, codes[part], &ok);
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
  int64_t jd = date_arg(ctx, ret, argc, argv, "DOW", 1115, &ok);
  if (ok) ret_number(ret, day_of_week(jd), 3);
}
