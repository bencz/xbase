/* Set( nSetting [, xNewValue] ): returns the setting's value before the call. */
#include "xbrt_int.h"

#include "set.ch"

#include "core/codepage.h"
#include "rtl/language.ch"

#include <stdlib.h>
#include <string.h>

/* Logical settings also accept "ON"/"OFF" (any case), the form SET commands
 * pass; any other value leaves the setting unchanged. */
static int set_logical(const xb_item *v, int old) {
  if (v->type == XB_T_LOGICAL) return v->v.l != 0;
  if (v->type == XB_T_STRING) {
    const xb_str *s = v->v.s;
    char c0 = s->len > 0 ? xb_ascii_lower(s->data[0]) : 0;
    char c1 = s->len > 1 ? xb_ascii_lower(s->data[1]) : 0;
    char c2 = s->len > 2 ? xb_ascii_lower(s->data[2]) : 0;
    if (c0 == 'o' && c1 == 'n') return 1;
    if (c0 == 'o' && c1 == 'f' && c2 == 'f') return 0;
  }
  return old;
}

static int is_year(char c) { return xb_ascii_lower(c) == 'y'; }

/* The first run of Ys of a date format: its length, and in `start` its
 * position (0 when there is none). */
static size_t year_run(const char *fmt, size_t *start) {
  size_t i = 0;
  while (fmt[i] && !is_year(fmt[i])) ++i;
  size_t n = 0;
  while (is_year(fmt[i + n])) ++n;
  *start = n ? i : 0;
  return n;
}

void xbf_SET(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || !xb_is_number(&argv[0])) return;
  int64_t which = xb_num_to_i64(&argv[0]);
  const xb_item *value = argc > 1 ? &argv[1] : NULL;

  switch (which) {
    case _SET_EXACT:
      xb_set_logical(ret, ctx->env->set_exact);
      if (value) ctx->env->set_exact = set_logical(value, ctx->env->set_exact);
      break;
    case _SET_FIXED:
      xb_set_logical(ret, ctx->env->set_fixed);
      if (value) ctx->env->set_fixed = set_logical(value, ctx->env->set_fixed);
      break;
    case _SET_DECIMALS:
      xb_set_int(ret, ctx->set_decimals);
      if (value && xb_is_number(value)) {
        int64_t n = xb_num_to_i64(value);
        if (n < 0) xb_error_in(ctx, "SET", NULL, EG_ARG, 2020, "SET", 0, argc, argv);
        else ctx->set_decimals = n > INT32_MAX ? INT32_MAX : (int32_t)n;
      }
      break;
    case _SET_DATEFORMAT:
      xb_set_str(ret, xb_str_new(ctx->env->date_format, strlen(ctx->env->date_format)));
      if (value && value->type == XB_T_STRING) {
        xb_ctx_set_date_format(ctx, value->v.s->data, value->v.s->len);
        size_t start;
        ctx->env->century = year_run(ctx->env->date_format, &start) >= 4;
      }
      break;
    case _SET_EPOCH:
      xb_set_int(ret, ctx->env->epoch);
      if (value && xb_is_number(value)) {
        int64_t n = xb_num_to_i64(value);
        if (n < 0) xb_error_in(ctx, "SET", NULL, EG_ARG, 2020, "SET", 0, argc, argv);
        else ctx->env->epoch = n > INT32_MAX ? INT32_MAX : (int32_t)n;
      }
      break;
    case _SET_DELETED:
      xb_set_logical(ret, ctx->env->set_deleted);
      if (value) ctx->env->set_deleted = set_logical(value, ctx->env->set_deleted);
      break;
    case _SET_EXCLUSIVE:
      xb_set_logical(ret, !ctx->env->set_shared);
      if (value) ctx->env->set_shared = !set_logical(value, !ctx->env->set_shared);
      break;
    case _SET_SOFTSEEK:
      xb_set_logical(ret, ctx->env->set_softseek);
      if (value) ctx->env->set_softseek = set_logical(value, ctx->env->set_softseek);
      break;
    case _SET_UNIQUE:
      xb_set_logical(ret, ctx->env->set_unique);
      if (value) ctx->env->set_unique = set_logical(value, ctx->env->set_unique);
      break;
    case _SET_LANGUAGE: { /* a name the runtime has no language for leaves it as it is */
      const xb_array *texts = xb_language(ctx);
      if (texts && texts->len >= LANG_ID) *ret = texts->items[LANG_ID - 1];
      if (value && value->type == XB_T_STRING) xb_language_select(ctx, value->v.s->data, value->v.s->len);
      break;
    }
    case _SET_CODEPAGE: { /* a name the runtime has no codepage for leaves it as it is */
      const xb_codepage *cp = xb_codepage_of(ctx);
      xb_set_str(ret, xb_str_new(cp->name, strlen(cp->name)));
      if (value && value->type == XB_T_STRING) {
        const xb_codepage *chosen = xb_codepage_find(value->v.s->data, value->v.s->len);
        if (chosen) ctx->env->codepage = chosen;
      }
      break;
    }
    default:
      break;
  }
}

/* __SetCentury( [lOn] ): returns the setting before the call. Changing it
 * rewrites the first run of Ys of the date format as YYYY or YY, or puts
 * one in front when there is none, and the format in upper case. */
void xbf___SETCENTURY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int old = ctx->env->century;
  xb_set_logical(ret, old);
  if (argc < 1) return;
  int now = set_logical(&argv[0], old);
  if (now == old) return;
  ctx->env->century = now;
  const char *old_fmt = ctx->env->date_format;
  const char *year = now ? "YYYY" : "YY";
  size_t start;
  size_t run = year_run(old_fmt, &start);
  size_t len = strlen(old_fmt), end = start + run;
  size_t n = len - (end - start) + strlen(year);
  char *fmt = malloc(n);
  if (!fmt) xb_gc_fatal("out of memory");
  size_t o = 0;
  for (size_t i = 0; i < start; ++i) fmt[o++] = xb_ascii_upper(old_fmt[i]);
  for (const char *y = year; *y; ++y) fmt[o++] = *y;
  for (size_t i = end; i < len; ++i) fmt[o++] = xb_ascii_upper(old_fmt[i]);
  xb_ctx_set_date_format(ctx, fmt, n);
  free(fmt);
}
