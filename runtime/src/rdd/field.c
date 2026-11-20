/* Field references of compiled code (xb_field_get, xb_field_set, xb_mv_field in xbrt.h) and alias->( expr )
 * (__AreaEval). */
#include "rdd/rdd.h"

#include "core/numeric.h"

#include <stdio.h>
#include <string.h>

void xb_field_decode(const xb_field_info *fi, const char *p, xb_item *r) {
  switch (fi->type) {
  case 'C':
    xb_set_str(r, xb_str_new(p, fi->len));
    return;
  case 'D':
    xb_set_date(r, xb_date_from_digits(p));
    return;
  case 'L':
    xb_set_logical(r, xb_field_true(p[0]));
    return;
  default: { /* N, F: the text's number, with the field's width and decimals */
    xb_num_parse(p, fi->len, r);
    if (fi->dec == 0 && r->type == XB_T_INT) {
      xb_put(r, xb_header_word(XB_T_INT, (uint16_t)fi->len, 0), (uint64_t)r->v.i);
      return;
    }
    double v = r->type == XB_T_INT ? (double)r->v.i : r->v.d;
    uint64_t bits;
    memcpy(&bits, &v, 8);
    uint32_t width = fi->dec ? (fi->len > fi->dec + 1 ? fi->len - fi->dec - 1 : 1) : fi->len;
    xb_put(r, xb_header_word(XB_T_DOUBLE, (uint16_t)width, (uint16_t)fi->dec), bits);
    return;
  }
  }
}

int xb_field_number_text(const char *s, size_t n, uint32_t dec) {
  size_t i = n && s[0] == '-' ? 1 : 0, digits = i;
  while (i < n && s[i] >= '0' && s[i] <= '9') ++i;
  if (i == digits) return 0;
  if (!dec) return i == n;
  if (i >= n || s[i] != '.' || n - i - 1 != dec) return 0;
  for (++i; i < n; ++i)
    if (s[i] < '0' || s[i] > '9') return 0;
  return 1;
}

int xb_field_encode(const xb_field_info *fi, char *p, const xb_item *v) {
  switch (fi->type) {
  case 'C': {
    if (v->type != XB_T_STRING) return XB_PUT_TYPE;
    size_t n = v->v.s->len < fi->len ? v->v.s->len : fi->len;
    memcpy(p, xb_str_data(v->v.s), n);
    memset(p + n, ' ', fi->len - n);
    return XB_PUT_OK;
  }
  case 'D': {
    if (v->type != XB_T_DATE) return XB_PUT_TYPE;
    char digits[XB_DATE_DIGITS_SIZE]; /* blanks for the empty date */
    xb_date_to_digits(v->v.i, digits);
    memcpy(p, digits, XB_DATE_DIGITS_SIZE - 1);
    return XB_PUT_OK;
  }
  case 'L':
    if (v->type != XB_T_LOGICAL) return XB_PUT_TYPE;
    p[0] = v->v.l ? 'T' : 'F';
    return XB_PUT_OK;
  default:
    if (!xb_is_number(v)) return XB_PUT_TYPE;
    xb_num_to_field_buf(v, (int)fi->len, (int)fi->dec, p);
    return p[0] == '*' ? XB_PUT_WIDTH : XB_PUT_OK; /* the stars stay, as in Clipper */
  }
}

/* A field reference's cache (xb_field_cache): one word, read and written atomically (a thread finds either its
 * table's entry whole or another one): the generation of the work area it found the field in (xb_area.generation: one
 * opening of a table), the area's number and the field's index; none past 2^32 openings. */
#define CACHE_GENERATION_SHIFT 32
#define CACHE_AREA_SHIFT 16
#define CACHE_PART 0xFFFFu

/* The text of an alias for its error ("Alias does not exist"). */
static void alias_text(const xb_item *area, const char *alias, char what[32]) {
  what[0] = '\0';
  if (area && area->type == XB_T_STRING)
    snprintf(what, 32, "%.*s", (int)(area->v.s->len < 31 ? area->v.s->len : 31), xb_str_data(area->v.s));
  else if (area && xb_is_number(area))
    snprintf(what, 32, "%lld", (long long)xb_num_to_i64(area));
  else if (alias)
    snprintf(what, 32, "%s", alias);
}

/* The number of the work area an alias names: the item `area` (a name as xb_area_named reads it, or a number from
 * 1; either names its area open or not), else the C string `alias` (NULL: the current area), whose area the cache
 * gives while that opening of the table lasts (no search by name; an alias in an item may name another area each
 * time). 0 when it names none. */
static uint32_t area_number(xb_ctx *ctx, const xb_item *area, const char *alias, const xb_field_cache *cache) {
  if (area && area->type == XB_T_STRING)
    return xb_area_number(xb_area_named(ctx, xb_str_data(area->v.s), area->v.s->len));
  if (area && xb_is_number(area)) return xb_area_number(xb_num_to_i64(area));
  if (area) return 0;
  if (!alias) return ctx->env->areaCurrent;
  uint64_t key = xb_atomic_load64(&cache->key, XB_RELAXED);
  uint32_t n = (uint32_t)(key >> CACHE_AREA_SHIFT) & CACHE_PART;
  const xb_area *a = key ? xb_area_at(ctx, n) : NULL;
  if (a && a->generation == key >> CACHE_GENERATION_SHIFT) return n;
  return xb_area_number(xb_area_named(ctx, alias, strlen(alias)));
}

/* The position of field `name` in `a`, through the reference's cache; -1 when the table has none. */
static int64_t field_of(xb_area *a, const char *name, xb_field_cache *cache) {
  uint64_t key = xb_atomic_load64(&cache->key, XB_RELAXED);
  if (key && key >> CACHE_GENERATION_SHIFT == a->generation && (key & CACHE_PART) < a->nfields)
    return (int64_t)(key & CACHE_PART);
  int64_t k = xb_area_field_index(a, name, strlen(name));
  if (k >= 0 && k <= CACHE_PART && a->number <= CACHE_PART && !(a->generation >> (64 - CACHE_GENERATION_SHIFT)))
    xb_atomic_store64(&cache->key,
                      a->generation << CACHE_GENERATION_SHIFT | (uint64_t)a->number << CACHE_AREA_SHIFT | (uint64_t)k,
                      XB_RELAXED);
  return k;
}

/* No area for the alias (`n` 0), or no field of the name: the error, raised; 1 when its block asks to try again. Out
 * of line: the found path stays short. */
static XB_NOINLINE int missing(xb_ctx *ctx, const xb_item *area, const char *alias, const char *name, uint32_t n,
                               uint16_t tries) {
  char what[32];
  alias_text(area, alias, what);
  int noAlias = n == 0;
  return xb_error_of(ctx, NULL, noAlias ? EG_NOALIAS : EG_NOVAR, noAlias ? 1002 : 1003, noAlias ? what : name, NULL,
                     0, XB_ERR_RETRY, 0, NULL, tries, NULL) == 2;
}

/* Finds the work area and the field: 1, or 0 after the error (raised; the block did not ask to try again). */
static int find(xb_ctx *ctx, const xb_item *area, const char *alias, const char *name, xb_field_cache *cache,
                xb_area **a, uint32_t *field) {
  for (uint16_t tries = 1;; ++tries) {
    uint32_t n = area_number(ctx, area, alias, cache);
    *a = xb_area_settled(ctx, n);
    int64_t k = *a ? field_of(*a, name, cache) : -1;
    if (k >= 0) {
      *field = (uint32_t)k;
      return 1;
    }
    if (!missing(ctx, area, alias, name, n, tries)) return 0;
  }
}

void xb_field_get(xb_ctx *ctx, xb_item *r, const xb_item *area, const char *alias, const char *name,
                  xb_field_cache *cache) {
  xb_area *a;
  uint32_t field;
  if (find(ctx, area, alias, name, cache, &a, &field)) xb_area_field_get(ctx, a, field, r);
}

void xb_field_set(xb_ctx *ctx, const xb_item *area, const char *alias, const char *name, xb_field_cache *cache,
                  const xb_item *v) {
  xb_area *a;
  uint32_t field;
  if (find(ctx, area, alias, name, cache, &a, &field) && !a->eof) xb_area_field_put(ctx, a, field, v);
}

int xb_field_try(xb_ctx *ctx, xb_item *r, const char *name, xb_field_cache *cache) {
  xb_area *a = xb_area_current(ctx);
  int64_t k = a ? field_of(a, name, cache) : -1;
  if (k >= 0) { /* a field: the record's pending relational move first, which may close the area */
    a = xb_area_record(ctx);
    k = a ? field_of(a, name, cache) : -1;
  }
  if (k < 0) return 0;
  xb_area_field_get(ctx, a, (uint32_t)k, r);
  return 1;
}

void xb_mv_field(xb_ctx *ctx, xb_item *r, const xb_memvar *m, xb_field_cache *cache) {
  if (!xb_field_try(ctx, r, m->name, cache)) xb_mv_get(ctx, r, m);
}

int xb_area_lookup(xb_ctx *ctx, const xb_item *area, uint32_t *n) {
  for (uint16_t tries = 1;; ++tries) {
    if ((*n = area_number(ctx, area, NULL, NULL)) != 0) return 1; /* no alias text: no cache */
    char what[32];
    alias_text(area, NULL, what);
    if (xb_error_of(ctx, NULL, EG_NOALIAS, 1002, what, NULL, 0, XB_ERR_RETRY, 0, NULL, tries, NULL) != 2) return 0;
  }
}

int xb_area_enter(xb_ctx *ctx, const xb_item *area, uint32_t *before) {
  uint32_t n;
  if (!xb_area_lookup(ctx, area, &n)) return 0;
  *before = ctx->env->areaCurrent;
  ctx->env->areaCurrent = n;
  return 1;
}

/* __AreaEval( xArea, bBlock ): bBlock's value, evaluated with the work area of the alias (or number) current; the
 * area current before comes back after it, also when the block fails. */
void xbf___AREAEVAL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  uint32_t before;
  if (argc < 2 || argv[1].type != XB_T_BLOCK || !xb_area_enter(ctx, &argv[0], &before)) return;
  xb_block_call(ctx, ret, &argv[1], 0, ret);
  ctx->env->areaCurrent = before;
}
