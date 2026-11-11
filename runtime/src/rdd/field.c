/* Field references of compiled code (xb_field_get, xb_field_set, xb_mv_field in xbrt.h) and alias->( expr )
 * (__AreaEval). */
#include "rdd/rdd.h"

#include <stdio.h>
#include <string.h>

/* The number of the work area an alias names: the item `area` (a name as xb_area_named reads it, or a number from
 * 1; either names its area open or not), else the C string `alias` (NULL: the current area). 0 when it names none;
 * `what` gets the alias's text for the error. */
static uint32_t area_number(xb_ctx *ctx, const xb_item *area, const char *alias, char what[32]) {
  what[0] = '\0';
  if (area && area->type == XB_T_STRING) {
    snprintf(what, 32, "%.*s", (int)(area->v.s->len < 31 ? area->v.s->len : 31), area->v.s->data);
    return xb_area_number(xb_area_named(ctx, area->v.s->data, area->v.s->len));
  }
  if (area && xb_is_number(area)) {
    int64_t n = xb_num_to_i64(area);
    snprintf(what, 32, "%lld", (long long)n);
    return xb_area_number(n);
  }
  if (alias) {
    snprintf(what, 32, "%s", alias);
    return xb_area_number(xb_area_named(ctx, alias, strlen(alias)));
  }
  if (area) return 0;
  return ctx->areaCurrent;
}

/* The position of field `name` in `a`, through the reference's cache; -1 when the table has none. */
static int64_t field_of(xb_area *a, const char *name, xb_field_cache *cache) {
  if (cache->generation == a->generation && cache->index < a->nfields) return cache->index;
  int64_t k = xb_area_field_index(a, name, strlen(name));
  if (k >= 0) {
    cache->generation = a->generation;
    cache->index = (uint32_t)k;
  }
  return k;
}

/* Finds the work area and the field: 1, or 0 after the error (raised; the block did not ask to try again). */
static int find(xb_ctx *ctx, const xb_item *area, const char *alias, const char *name, xb_field_cache *cache,
                xb_area **a, uint32_t *field) {
  for (uint16_t tries = 1;; ++tries) {
    char what[32];
    uint32_t n = area_number(ctx, area, alias, what);
    *a = xb_area_at(ctx, n);
    int64_t k = *a ? field_of(*a, name, cache) : -1;
    if (k >= 0) {
      *field = (uint32_t)k;
      return 1;
    }
    int noAlias = n == 0;
    int r = xb_error_of(ctx, NULL, noAlias ? EG_NOALIAS : EG_NOVAR, noAlias ? 1002 : 1003, noAlias ? what : name,
                        NULL, 0, XB_ERR_RETRY, 0, NULL, tries, NULL);
    if (r != 2) return 0;
  }
}

void xb_field_get(xb_ctx *ctx, xb_item *r, const xb_item *area, const char *alias, const char *name,
                  xb_field_cache *cache) {
  xb_area *a;
  uint32_t field;
  if (find(ctx, area, alias, name, cache, &a, &field)) a->rdd->get(a, field, r);
}

void xb_field_set(xb_ctx *ctx, const xb_item *area, const char *alias, const char *name, xb_field_cache *cache,
                  const xb_item *v) {
  xb_area *a;
  uint32_t field;
  if (find(ctx, area, alias, name, cache, &a, &field) && !a->eof) xb_area_field_put(ctx, a, field, v);
}

void xb_mv_field(xb_ctx *ctx, xb_item *r, const xb_memvar *m, xb_field_cache *cache) {
  xb_area *a = xb_area_current(ctx);
  int64_t k = a ? field_of(a, m->name, cache) : -1;
  if (k >= 0) a->rdd->get(a, (uint32_t)k, r);
  else xb_mv_get(ctx, r, m);
}

int xb_area_enter(xb_ctx *ctx, const xb_item *area, uint32_t *before) {
  uint32_t n;
  for (uint16_t tries = 1;; ++tries) {
    char what[32];
    if ((n = area_number(ctx, area, NULL, what)) != 0) break;
    if (xb_error_of(ctx, NULL, EG_NOALIAS, 1002, what, NULL, 0, XB_ERR_RETRY, 0, NULL, tries, NULL) != 2) return 0;
  }
  *before = ctx->areaCurrent;
  ctx->areaCurrent = n;
  return 1;
}

/* __AreaEval( xArea, bBlock ): bBlock's value, evaluated with the work area of the alias (or number) current; the
 * area current before comes back after it, also when the block fails. */
void xbf___AREAEVAL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  uint32_t before;
  if (argc < 2 || argv[1].type != XB_T_BLOCK || !xb_area_enter(ctx, &argv[0], &before)) return;
  xb_block_call(ctx, ret, &argv[1], 0, ret);
  ctx->areaCurrent = before;
}
