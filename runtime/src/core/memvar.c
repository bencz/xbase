/* Memory variables: PRIVATE and PUBLIC, found by name at run time (see the
 * xb_mv_* functions in xbrt.h). Names get numbers when modules register; each
 * context keeps, by number, the binding visible now, and the PRIVATEs alive
 * with the bindings they hid. A binding is a cell (an XB_T_CELL item): what an
 * argument @name passes, and what PARAMETERS shares with an argument passed by
 * reference (a cell holding an element's reference reaches the element). */
#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

/* The names numbered so far, filled while modules register, before the
 * program runs. */
static xb_names names;

void xb_memvars_add(xb_memvar *memvars, uint32_t n) {
  for (uint32_t k = 0; k < n; ++k) memvars[k].id = xb_names_add(&names, memvars[k].name, strlen(memvars[k].name));
}

/* The binding of variable `id` in the context: NIL for none. */
static xb_item *binding(xb_ctx *ctx, uint32_t id) {
  if (id >= ctx->memvarCap) {
    uint32_t cap = names.count; /* every name numbered so far */
    xb_item *mv = realloc(ctx->memvars, cap * sizeof *mv);
    if (!mv) xb_gc_fatal("out of memory");
    for (uint32_t i = ctx->memvarCap; i < cap; ++i) xb_set_nil(&mv[i]);
    ctx->memvars = mv;
    ctx->memvarCap = cap;
  }
  return &ctx->memvars[id];
}

/* A new binding for `id`, holding `value` (or the cell of a variable passed by
 * reference), recorded as a PRIVATE of the activation that runs now. */
static void push_private(xb_ctx *ctx, uint32_t id, const xb_item *value) {
  if (ctx->privateLen == ctx->privateCap) {
    uint32_t cap = ctx->privateCap ? ctx->privateCap * 2 : 32;
    xb_private *p = cap > ctx->privateCap ? realloc(ctx->privates, cap * sizeof *p) : NULL;
    if (!p) xb_gc_fatal("out of memory");
    ctx->privates = p;
    ctx->privateCap = cap;
  }
  xb_item fresh;
  if (value->type == XB_T_CELL) {
    fresh = *value; /* PARAMETERS: the caller's variable itself */
  } else {
    xb_gc_root_push(ctx, (xb_item *)value);
    xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell));
    xb_gc_root_pop(ctx, 1);
    c->value = *value; /* young: no barrier */
    xb_set_ptr(&fresh, XB_T_CELL, c);
  }
  xb_item *b = binding(ctx, id);
  xb_private *p = &ctx->privates[ctx->privateLen++];
  p->id = id;
  p->hidden = *b;
  *b = fresh;
}

/* Whether the activation whose mark is `mark` created a PRIVATE `id`. */
static int activation_has(const xb_ctx *ctx, uint32_t id, uint32_t mark) {
  for (uint32_t i = ctx->privateLen; i-- > mark;)
    if (ctx->privates[i].id == id) return 1;
  return 0;
}

uint32_t xb_mv_mark(xb_ctx *ctx) { return ctx->privateLen; }

void xb_mv_release(xb_ctx *ctx, uint32_t mark) {
  while (ctx->privateLen > mark) {
    xb_private *p = &ctx->privates[--ctx->privateLen];
    ctx->memvars[p->id] = p->hidden;
  }
}

/* The binding visible for `m`; with none, error 1003, which the block may
 * retry: NULL when it does not. */
static xb_item *visible(xb_ctx *ctx, const xb_memvar *m) {
  for (uint16_t tries = 1;; ++tries) {
    xb_item *b = binding(ctx, m->id);
    if (b->type == XB_T_CELL) return b;
    if (!xb_error_retry(ctx, EG_NOVAR, 1003, m->name, tries)) return NULL;
  }
}

void xb_mv_get(xb_ctx *ctx, xb_item *r, const xb_memvar *m) {
  xb_item *b = visible(ctx, m);
  if (!b) return;
  const xb_item *v = xb_ref_target(ctx, b);
  if (v) *r = *v;
}

void xb_mv_ref(xb_ctx *ctx, xb_item *r, const xb_memvar *m) {
  xb_item *b = visible(ctx, m);
  if (b) *r = *b;
}

void xb_mv_set(xb_ctx *ctx, const xb_memvar *m, const xb_item *v) {
  xb_item *b = binding(ctx, m->id);
  if (b->type != XB_T_CELL) {
    push_private(ctx, m->id, v);
    return;
  }
  xb_item *target = xb_ref_target(ctx, b);
  if (!target) return;
  *target = *v;
  xb_ref_written(b);
}

void xb_mv_private(xb_ctx *ctx, const xb_memvar *m, uint32_t mark) {
  if (activation_has(ctx, m->id, mark)) return;
  xb_item nil;
  xb_set_nil(&nil);
  push_private(ctx, m->id, &nil);
}

void xb_mv_public(xb_ctx *ctx, const xb_memvar *m) {
  if (binding(ctx, m->id)->type == XB_T_CELL) return;
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell));
  xb_set_logical(&c->value, strcmp(m->name, "CLIPPER") == 0); /* Clipper's rule: PUBLIC Clipper is .T. */
  xb_set_ptr(binding(ctx, m->id), XB_T_CELL, c); /* not a PRIVATE: no activation ends it */
}

void xb_mv_param(xb_ctx *ctx, const xb_memvar *m, const xb_item *arg) { push_private(ctx, m->id, arg); }
