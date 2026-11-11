/* Work areas: the table of each task's areas, aliases, and navigation as the language sees it (see rdd.h). */
#include "rdd/rdd.h"

#include "rdd/order.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The names of the drivers: DBFNTX is the DBF driver until it has indexes. */
static const struct {
  const char *alias;
  const xb_rdd *rdd;
} driver_names[] = {{"DBF", &xb_rdd_dbf}, {"DBFNTX", &xb_rdd_dbf}};

static int same_upper(const char *a, size_t len, const char *upper) {
  for (size_t i = 0; i < len; ++i)
    if (!upper[i] || xb_ascii_upper(a[i]) != upper[i]) return 0;
  return upper[len] == '\0';
}

const xb_rdd *xb_rdd_find(const char *name, size_t len) {
  for (size_t k = 0; k < sizeof driver_names / sizeof driver_names[0]; ++k)
    if (same_upper(name, len, driver_names[k].alias)) return driver_names[k].rdd;
  return NULL;
}

const xb_rdd *xb_rdd_default(xb_ctx *ctx) { return ctx->rddDefault ? ctx->rddDefault : &xb_rdd_dbf; }

xb_area *xb_area_at(xb_ctx *ctx, uint32_t n) {
  return n >= 1 && n <= ctx->areaCap ? ctx->areas[n - 1] : NULL;
}

xb_area *xb_area_current(xb_ctx *ctx) { return xb_area_at(ctx, ctx->areaCurrent); }

uint32_t xb_area_number(int64_t n) { return n >= 1 && n <= XB_AREA_MAX ? (uint32_t)n : 0; }

/* The area of an alias (any letter case, `len` bytes), 0 for none. */
static uint32_t area_of_alias(xb_ctx *ctx, const char *alias, size_t len) {
  for (uint32_t k = 0; k < ctx->areaCap; ++k)
    if (ctx->areas[k] && same_upper(alias, len, ctx->areas[k]->alias)) return k + 1;
  return 0;
}

/* The area a one-letter name gives, A to K (areas 1 to 11); 0 for another name. */
static uint32_t letter_area(const char *s, size_t len) {
  char letter = len == 1 ? xb_ascii_upper(s[0]) : 0;
  return letter >= 'A' && letter <= 'K' ? (uint32_t)(letter - 'A' + 1) : 0;
}

static size_t without_trailing_blanks(const char *s, size_t len) {
  while (len > 0 && s[len - 1] == ' ') --len;
  return len;
}

int64_t xb_area_named(xb_ctx *ctx, const char *s, size_t len) {
  size_t i = 0;
  while (i < len && s[i] == ' ') ++i;
  if (i < len && s[i] >= '0' && s[i] <= '9') {
    int64_t n = 0;
    for (; i < len && s[i] >= '0' && s[i] <= '9' && n <= XB_AREA_MAX; ++i) n = n * 10 + (s[i] - '0');
    return n <= XB_AREA_MAX ? n : -1;
  }
  len = without_trailing_blanks(s, len);
  uint32_t n = letter_area(s, len);
  if (!n) n = area_of_alias(ctx, s, len);
  return n ? (int64_t)n : -1;
}

uint32_t xb_area_free(xb_ctx *ctx) {
  uint32_t n = 1;
  while (xb_area_at(ctx, n)) ++n;
  return n;
}

void xb_area_error(xb_ctx *ctx, const xb_area *a, int32_t gencode, int32_t subcode, const char *operation,
                   int32_t oscode) {
  xb_error_of(ctx, a->rdd->name, gencode, subcode, operation, NULL, oscode, 0, 0, NULL, 0, NULL);
}

void xb_area_unused_error(xb_ctx *ctx, const char *fn) {
  xb_error_of(ctx, XB_DBCMD, EG_NOTABLE, 2001, fn, NULL, 0, 0, 0, NULL, 0, NULL);
}

int xb_area_writable(xb_ctx *ctx, xb_area *a) {
  if (!a->readonly) return 1;
  xb_area_error(ctx, a, EG_READONLY, 1025, "", 0);
  return 0;
}

void xb_order_conditions_clear(xb_order_conditions *c) {
  memset(c, 0, sizeof *c);
  xb_set_nil(&c->cond);
  xb_set_nil(&c->condText);
  xb_set_nil(&c->whileBlock);
  xb_set_nil(&c->evalBlock);
}

void xb_area_pin(xb_area *a) { ++a->pins; }

static void area_free(xb_area *a) {
  free(a->alias);
  free(a);
}

void xb_area_unpin(xb_area *a) {
  if (--a->pins == 0 && a->closed) area_free(a);
}

/* Closes the table of `a`, already taken out of the table of areas, then raises a close error. */
static void close_table(xb_ctx *ctx, xb_area *a) {
  int32_t err = 0;
  int ok = a->rdd->close(a, &err);
  a->nfields = 0;
  a->closed = 1;
  if (!ok) xb_area_error(ctx, a, EG_CLOSE, 1012, "", err);
  if (!a->pins) area_free(a);
}

void xb_area_close(xb_ctx *ctx, uint32_t n) {
  xb_area *a = xb_area_at(ctx, n);
  if (!a) return;
  xb_area_pin(a); /* the keys of a changed record are evaluated in the area, which they may close */
  xb_area_orders_close(ctx, a);
  int still = !a->closed && xb_area_at(ctx, n) == a;
  xb_area_unpin(a);
  if (!still) return;
  ctx->areas[n - 1] = NULL;
  close_table(ctx, a);
}

int xb_area_orders_close(xb_ctx *ctx, xb_area *a) {
  int ok = xb_area_cold(ctx, a);
  if (a->closed) return 0;
  while (a->norders) {
    xb_order *o = a->orders[--a->norders];
    ok = xb_order_close(ctx, a, o) && ok;
  }
  free(a->orders);
  a->orders = NULL;
  a->order = 0;
  return ok;
}

xb_order *xb_area_order(xb_area *a) { return a->order ? a->orders[a->order - 1] : NULL; }

void xb_area_close_all(xb_ctx *ctx) {
  for (uint32_t n = 1; n <= ctx->areaCap; ++n) xb_area_close(ctx, n);
}

/* Whether `s` is a name an alias can be: a letter or `_`, then letters, digits and `_`. */
static int alias_name(const char *s, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    char c = xb_ascii_upper(s[i]);
    if (!((c >= 'A' && c <= 'Z') || c == '_' || (i > 0 && c >= '0' && c <= '9'))) return 0;
  }
  return len > 0;
}

int xb_area_open(xb_ctx *ctx, uint32_t n, const xb_rdd *rdd, const char *path, const char *alias, size_t aliasLen,
                 int readonly) {
  static uint32_t generations;
  xb_area_close(ctx, n);
  xb_area *a = calloc(1, sizeof *a);
  if (!a) xb_gc_fatal("out of memory");
  a->number = n;
  a->rdd = rdd;
  a->readonly = readonly;
  a->generation = ++generations;
  xb_order_conditions_clear(&a->pending);
  for (uint16_t tries = 1;; ++tries) {
    xb_open_failure why = {0, EG_OPEN, 1001, NULL};
    if (rdd->open(a, path, readonly, &why)) break;
    int r = xb_error_of(ctx, rdd->name, why.gencode, why.subcode, "", why.file ? why.file : path, why.oscode,
                        XB_ERR_RETRY | XB_ERR_DEFAULT, 0, NULL, tries, NULL);
    free(why.file);
    if (r != 2) {
      free(a);
      return 0;
    }
  }
  aliasLen = without_trailing_blanks(alias, aliasLen);
  int legal = alias_name(alias, aliasLen);
  uint32_t taken = legal ? area_of_alias(ctx, alias, aliasLen) : 0;
  if (!legal || (taken && taken != n) || letter_area(alias, aliasLen) || same_upper(alias, aliasLen, "M")) {
    char text[64];
    snprintf(text, sizeof text, "%.*s", (int)(aliasLen < 63 ? aliasLen : 63), alias);
    close_table(ctx, a);
    xb_error_of(ctx, XB_DBCMD, legal ? EG_DUPALIAS : EG_BADALIAS, legal ? 1011 : 1010, text, NULL, 0, XB_ERR_DEFAULT,
                0, NULL, 0, NULL);
    return 0;
  }
  a->alias = malloc(aliasLen + 1);
  if (!a->alias) xb_gc_fatal("out of memory");
  for (size_t i = 0; i < aliasLen; ++i) a->alias[i] = xb_ascii_upper(alias[i]);
  a->alias[aliasLen] = '\0';
  xb_set_nil(&a->filter);
  xb_set_str(&a->filter_text, xb_str_empty());
  xb_set_nil(&a->locate);
  xb_area_close(ctx, n); /* what an error block opened there meanwhile */
  if (n > ctx->areaCap) {
    uint32_t cap = ctx->areaCap ? ctx->areaCap : 16;
    while (cap < n) cap *= 2;
    xb_area **grown = realloc(ctx->areas, cap * sizeof *grown);
    if (!grown) xb_gc_fatal("out of memory");
    memset(grown + ctx->areaCap, 0, (cap - ctx->areaCap) * sizeof *grown);
    ctx->areas = grown;
    ctx->areaCap = cap;
  }
  ctx->areas[n - 1] = a;
  return xb_area_top(ctx, a);
}

void xb_areas_mark(xb_ctx *ctx, int (*mark)(const xb_item *)) {
  for (uint32_t k = 0; k < ctx->areaCap; ++k)
    if (ctx->areas[k]) {
      mark(&ctx->areas[k]->filter);
      mark(&ctx->areas[k]->filter_text);
      mark(&ctx->areas[k]->locate);
      const xb_order_conditions *c = &ctx->areas[k]->pending;
      mark(&c->cond);
      mark(&c->condText);
      mark(&c->whileBlock);
      mark(&c->evalBlock);
      for (uint32_t i = 0; i < ctx->areas[k]->norders; ++i) xb_order_mark(ctx->areas[k]->orders[i], mark);
    }
}

/* Makes `recno` the current record, raw: the blank record after the last outside 1 .. lastrec. A record that cannot
 * be read leaves the area there too. */
static int go_raw(xb_ctx *ctx, xb_area *a, uint32_t recno) {
  int32_t err = 0;
  if (!xb_area_cold(ctx, a) || a->closed) return 0;
  uint32_t last = a->rdd->lastrec(a);
  a->found = 0;
  if (!a->rdd->flush(a, 0, &err)) {
    xb_area_error(ctx, a, EG_WRITE, 1011, "", err);
    return 0;
  }
  if (recno < 1 || recno > last) recno = last + 1;
  int ok = a->rdd->go(a, recno, &err);
  a->recno = ok ? recno : last + 1;
  a->eof = a->recno > last;
  if (!ok) xb_area_error(ctx, a, EG_READ, 1010, "", err);
  return ok;
}

int xb_area_visible(xb_ctx *ctx, xb_area *a) {
  if (ctx->set_deleted && (!xb_area_load(ctx, a) || a->closed)) return -1;
  if (ctx->set_deleted && a->rdd->deleted(a)) return 0;
  if (a->filter.type != XB_T_BLOCK) return 1;
  xb_item r, block = a->filter;
  xb_set_nil(&r);
  xb_gc_root_push(ctx, &r);
  xb_gc_root_push(ctx, &block);
  xb_block_call(ctx, &r, &block, 0, &r); /* no arguments; argv stays a valid address */
  xb_gc_root_pop(ctx, 2);
  if ((ctx->pending && xb_pending(ctx)) || a->closed) return -1;
  return r.type != XB_T_LOGICAL || r.v.l;
}

/* The record after (`dir` 1) or before (-1) the current one in the controlling order (the record order when there
 * is none; from the blank record after the last, the last one): 1 with it in `recno`, 0 for none, -1 after an error
 * (raised). */
static int step(xb_ctx *ctx, xb_area *a, int dir, uint32_t *recno) {
  xb_order *o = xb_area_order(a);
  if (o) return xb_order_step(ctx, a, o, dir, recno);
  uint32_t last = a->rdd->lastrec(a);
  if (a->eof) *recno = dir < 0 ? last : 0;
  else *recno = dir < 0 ? a->recno - 1 : a->recno + 1;
  return *recno >= 1 && *recno <= last;
}

/* The first (`dir` 1) or last (-1) record of the controlling order, as step(). */
static int edge(xb_ctx *ctx, xb_area *a, int dir, uint32_t *recno) {
  xb_order *o = xb_area_order(a);
  if (o) return xb_order_edge(ctx, a, o, dir, recno);
  uint32_t last = a->rdd->lastrec(a);
  *recno = dir > 0 ? 1 : last;
  return last > 0;
}

/* Goes to the record a step() or edge() gave (`r` its result): the blank record after the last for none. */
static int arrive(xb_ctx *ctx, xb_area *a, int r, uint32_t recno) {
  if (r < 0 || a->closed) return -1;
  return go_raw(ctx, a, r ? recno : 0) ? r : -1;
}

/* From the current record, the first that shows going `dir` (+1 or -1), the record itself included: 1 found, 0 none
 * (left on the blank record after the last), -1 after an error. */
static int seek_visible(xb_ctx *ctx, xb_area *a, int dir) {
  for (;;) {
    if (a->eof) return 0;
    int v = xb_area_visible(ctx, a);
    if (v) return v;
    uint32_t recno;
    int r = step(ctx, a, dir, &recno);
    r = arrive(ctx, a, r, recno);
    if (r <= 0) return r;
  }
}

int xb_area_edge_raw(xb_ctx *ctx, xb_area *a, int dir) {
  if (!xb_area_cold(ctx, a) || a->closed) return -1;
  uint32_t recno;
  int r = edge(ctx, a, dir, &recno);
  return arrive(ctx, a, r, recno);
}

int xb_area_step_raw(xb_ctx *ctx, xb_area *a, int dir) {
  if (!xb_area_cold(ctx, a) || a->closed) return -1;
  uint32_t recno;
  xb_order *o = xb_area_order(a);
  int r = o ? xb_order_step_raw(ctx, a, o, dir, &recno) : step(ctx, a, dir, &recno);
  return arrive(ctx, a, r, recno);
}

int xb_area_goto(xb_ctx *ctx, xb_area *a, uint32_t recno) {
  if (!go_raw(ctx, a, recno)) return 0;
  a->bof = a->eof; /* a record that is not there: both */
  return 1;
}

/* The first (`dir` 1) or last (-1) record that shows; `a` pinned. */
static int go_end(xb_ctx *ctx, xb_area *a, int dir) {
  if (!xb_area_cold(ctx, a) || a->closed) return 0; /* the record's entries before the order is read */
  uint32_t recno;
  int e = edge(ctx, a, dir, &recno);
  if (arrive(ctx, a, e, recno) < 0) return 0;
  int r = seek_visible(ctx, a, dir);
  if (r < 0) return 0;
  a->bof = a->eof;
  return 1;
}

int xb_area_top(xb_ctx *ctx, xb_area *a) {
  xb_area_pin(a);
  int ok = go_end(ctx, a, 1);
  xb_area_unpin(a);
  return ok;
}

int xb_area_bottom(xb_ctx *ctx, xb_area *a) {
  xb_area_pin(a);
  int ok = go_end(ctx, a, -1);
  xb_area_unpin(a);
  return ok;
}

int xb_area_arrive(xb_ctx *ctx, xb_area *a, uint32_t recno, int dir) {
  xb_area_pin(a);
  int r = go_raw(ctx, a, recno) ? seek_visible(ctx, a, dir) : -1;
  int ok = r >= 0;
  if (r == 0 && dir < 0 && recno) { /* none shows before it: BOF, on the first record that shows */
    ok = go_end(ctx, a, 1);
    a->bof = 1;
  } else if (ok) {
    a->bof = a->eof;
  }
  xb_area_unpin(a);
  return ok;
}

/* xb_area_skip with `a` pinned. */
static int skip(xb_ctx *ctx, xb_area *a, int64_t n) {
  if (n == 0) return go_raw(ctx, a, a->recno);
  if (!xb_area_cold(ctx, a) || a->closed) return 0; /* the record's entries before the order is read */
  uint32_t recno;
  if (n > 0) {
    a->bof = 0;
    for (; n > 0 && !a->eof; --n) {
      int r = step(ctx, a, 1, &recno);
      if (arrive(ctx, a, r, recno) < 0) return 0;
      if (seek_visible(ctx, a, 1) < 0) return 0;
    }
    return 1;
  }
  for (; n < 0; ++n) {
    int r = step(ctx, a, -1, &recno);
    r = arrive(ctx, a, r, recno);
    if (r > 0) r = seek_visible(ctx, a, -1);
    if (r < 0) return 0;
    if (r == 0) { /* none before: BOF, on the first record that shows */
      if (!go_end(ctx, a, 1)) return 0;
      a->bof = 1;
      return 1;
    }
    a->eof = 0;
  }
  return 1;
}

int xb_area_skip(xb_ctx *ctx, xb_area *a, int64_t n) {
  xb_area_pin(a);
  int ok = skip(ctx, a, n);
  xb_area_unpin(a);
  return ok;
}

int xb_area_seek(xb_ctx *ctx, xb_area *a, const xb_item *v, int soft, int last) {
  xb_order *o = xb_area_order(a);
  unsigned char key[XB_NTX_KEY_MAX];
  size_t used;
  if (!o) {
    xb_error_of(ctx, a->rdd->name, EG_NOORDER, 1201, "", NULL, 0, XB_ERR_DEFAULT, 0, NULL, 0, NULL);
    return 0;
  }
  if (!xb_order_value_key(o, v, key, &used)) { /* a value of another type than the key's finds nothing */
    if (!go_raw(ctx, a, 0)) return 0;
    a->bof = 0;
    return 1;
  }
  xb_area_pin(a);
  int ok = 0, match = 0;
  uint32_t recno;
  if (xb_area_cold(ctx, a) && !a->closed) { /* LAST: the last entry at or before the key, the end for none */
    int r = xb_order_seek(ctx, a, o, key, used, last, &recno, &match);
    r = arrive(ctx, a, r, recno);
    if (r > 0) r = seek_visible(ctx, a, last ? -1 : 1);
    ok = r >= 0;
    match = r > 0 && xb_order_cursor_matches(o, key, used);
    if (ok && !match && (!soft || (last && r == 0))) ok = go_raw(ctx, a, 0);
    a->bof = 0;
    a->found = ok && match;
  }
  xb_area_unpin(a);
  return ok;
}

int64_t xb_area_field_index(const xb_area *a, const char *name, size_t len) {
  for (uint32_t k = 0; k < a->nfields; ++k)
    if (same_upper(name, len, a->fields[k].name)) return k;
  return -1;
}

int xb_area_load(xb_ctx *ctx, xb_area *a) {
  int32_t err = 0;
  if (a->rdd->load(a, &err)) return 1;
  a->recno = a->rdd->lastrec(a) + 1; /* the driver is on the blank record after the last */
  a->bof = a->eof = 1;
  xb_area_error(ctx, a, EG_READ, 1010, "", err);
  return 0;
}

int xb_area_field_get(xb_ctx *ctx, xb_area *a, uint32_t field, xb_item *r) {
  int32_t err = 0;
  if (a->rdd->get(a, field, r, &err)) return 1;
  xb_set_nil(r);
  xb_area_error(ctx, a, EG_READ, 1010, "", err);
  return 0;
}

int xb_area_field_put(xb_ctx *ctx, xb_area *a, uint32_t field, const xb_item *v) {
  if (!xb_area_writable(ctx, a) || !xb_area_hot(ctx, a, 0) || a->closed) return 0;
  int32_t err = 0;
  int r = a->rdd->put(a, field, v, &err);
  if (r == XB_PUT_OK) return 1;
  if (r == XB_PUT_FAILED) {
    xb_area_error(ctx, a, EG_WRITE, 1011, "", err);
    return 0;
  }
  int32_t subcode = r == XB_PUT_TYPE ? 1020 : 1021;
  return xb_error_of(ctx, a->rdd->name, r == XB_PUT_TYPE ? EG_DATATYPE : EG_DATAWIDTH, subcode, a->fields[field].name,
                     NULL, 0, XB_ERR_DEFAULT, 1, v, 0, NULL) == 1;
}
