/* Memory variables: PRIVATE and PUBLIC, found by name at run time (see the
 * xb_mv_* functions in xbrt.h). Names get numbers when modules register, and
 * when RESTORE meets a name no module uses, while the program runs: so
 * ctx->memvars may move whenever xBase code runs (an error block). Each
 * context keeps, by number, the binding visible now, and the PRIVATEs alive
 * with the bindings they hid.
 * A binding is a cell (an XB_T_CELL item): what an argument @name passes, and
 * what PARAMETERS shares with an argument passed by reference (a cell holding
 * an element's reference reaches the element). */
#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

/* The names numbered so far, and each name by number. */
static xb_names names;
static struct spelling {
  const char *name;
  size_t len;
} *spelled;
static uint32_t spelledCap;

/* xb_private.id of a PRIVATE that CLEAR MEMORY released: it no longer names a
 * variable, and restores nothing when its activation ends. */
#define CLEARED UINT32_MAX

/* The number of `name` (upper case, kept by reference), new when it had none. */
static uint32_t number(const char *name, size_t len) {
  uint32_t id = xb_names_add(&names, name, len);
  if (id == spelledCap) {
    uint32_t cap = spelledCap ? spelledCap * 2 : 64;
    struct spelling *s = cap > spelledCap ? realloc(spelled, cap * sizeof *s) : NULL;
    if (!s) xb_gc_fatal("out of memory");
    spelled = s;
    spelledCap = cap;
  }
  spelled[id] = (struct spelling){name, len};
  return id;
}

void xb_memvars_add(xb_memvar *memvars, uint32_t n) {
  for (uint32_t k = 0; k < n; ++k) memvars[k].id = number(memvars[k].name, strlen(memvars[k].name));
}

uint32_t xb_memvar_count(void) { return names.count; }

const char *xb_memvar_name(uint32_t id, size_t *len) {
  *len = spelled[id].len;
  return spelled[id].name;
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

/* A new binding of its own, holding `value`, stored in `*r`. */
static void new_binding(xb_ctx *ctx, xb_item *r, const xb_item *value) {
  xb_gc_root_push(ctx, (xb_item *)value);
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell));
  xb_gc_root_pop(ctx, 1);
  c->value = *value; /* young: no barrier */
  xb_set_ptr(r, XB_T_CELL, c);
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
  int borrowed = value->type == XB_T_CELL; /* PARAMETERS: the variable passed */
  xb_item fresh;
  if (borrowed) fresh = *value;
  else new_binding(ctx, &fresh, value);
  xb_item *b = binding(ctx, id);
  xb_private *p = &ctx->privates[ctx->privateLen++];
  p->id = id;
  p->borrowed = (uint32_t)borrowed;
  p->owner = ctx->frames;
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
    if (p->id != CLEARED) ctx->memvars[p->id] = p->hidden;
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

/* Variable `id` takes `v`: the one visible, else a new PRIVATE. The binding
 * is copied and rooted: reaching an element may run the error block, which
 * may create variables (moving ctx->memvars) or release this one. */
static void assign(xb_ctx *ctx, uint32_t id, const xb_item *v) {
  xb_item b = *binding(ctx, id);
  if (b.type != XB_T_CELL) {
    push_private(ctx, id, v);
    return;
  }
  xb_gc_root_push(ctx, &b);
  xb_item *target = xb_ref_target(ctx, &b);
  if (target && !(ctx->pending & XB_PENDING_UNWIND)) {
    *target = *v;
    xb_ref_written(&b);
  }
  xb_gc_root_pop(ctx, 1);
}

void xb_mv_set(xb_ctx *ctx, const xb_memvar *m, const xb_item *v) { assign(ctx, m->id, v); }

void xb_mv_private(xb_ctx *ctx, const xb_memvar *m, uint32_t mark) {
  if (activation_has(ctx, m->id, mark)) return;
  xb_item nil;
  xb_set_nil(&nil);
  push_private(ctx, m->id, &nil);
}

void xb_mv_public(xb_ctx *ctx, const xb_memvar *m) {
  xb_item *b = binding(ctx, m->id);
  if (b->type == XB_T_CELL) return;
  xb_item value;
  xb_set_logical(&value, strcmp(m->name, "CLIPPER") == 0); /* Clipper's rule: PUBLIC Clipper is .T. */
  new_binding(ctx, b, &value); /* not a PRIVATE: no activation ends it */
}

void xb_mv_param(xb_ctx *ctx, const xb_memvar *m, const xb_item *arg) { push_private(ctx, m->id, arg); }

const xb_item *xb_memvar_value(xb_ctx *ctx, uint32_t id) {
  xb_item *b = binding(ctx, id);
  return b->type == XB_T_CELL ? xb_ref_target(ctx, b) : NULL;
}

void xb_memvar_assign(xb_ctx *ctx, const char *name, size_t len, const xb_item *value) {
  uint32_t id;
  if (!xb_names_find_upper(&names, name, len, &id)) {
    char *upper = malloc(len ? len : 1); /* a name no module uses: kept for the program's life */
    if (!upper) xb_gc_fatal("out of memory");
    for (size_t i = 0; i < len; ++i) upper[i] = xb_ascii_upper(name[i]);
    id = number(upper, len);
  }
  assign(ctx, id, value);
}

/* Whether cell `c` is a memory variable's binding (visible, or hidden by a
 * PRIVATE) anywhere but at `self`. */
static int memvar_cell(const xb_ctx *ctx, const void *c, const xb_item *self) {
  for (uint32_t i = 0; i < ctx->memvarCap; ++i) {
    const xb_item *b = &ctx->memvars[i];
    if (b != self && b->type == XB_T_CELL && b->v.p == c) return 1;
  }
  for (uint32_t i = 0; i < ctx->privateLen; ++i) {
    const xb_item *b = &ctx->privates[i].hidden;
    if (b != self && b->type == XB_T_CELL && b->v.p == c) return 1;
  }
  return 0;
}

/* PRIVATE `i` holds NIL. A PARAMETERS one that received a LOCAL, a STATIC or
 * an element by reference lets it go for a variable of its own; any other is
 * a variable that becomes NIL where it is (a memory variable passed to
 * PARAMETERS by reference, and what was passed @name, see NIL too). */
static void private_to_nil(xb_ctx *ctx, uint32_t i) {
  xb_private *p = &ctx->privates[i];
  xb_item *b = NULL; /* where its binding is: in a later PRIVATE that hides it, else visible */
  for (uint32_t j = i + 1; j < ctx->privateLen && !b; ++j)
    if (ctx->privates[j].id == p->id) b = &ctx->privates[j].hidden;
  if (!b) b = binding(ctx, p->id);
  xb_item nil;
  xb_set_nil(&nil);
  if (p->borrowed && !memvar_cell(ctx, b->v.p, b)) {
    new_binding(ctx, b, &nil);
    p->borrowed = 0;
  } else {
    xb_item *target = xb_ref_target(ctx, b);
    if (target) *target = nil; /* NIL holds no reference: no barrier */
  }
}

int xb_memvar_like(const xb_str *skel, const char *name, size_t len) {
  size_t s = 0, n = 0, star = SIZE_MAX, resume = 0;
  while (n < len) {
    if (s < skel->len && skel->data[s] == '*') {
      star = ++s;
      resume = n;
    } else if (s < skel->len && (skel->data[s] == '?' || xb_ascii_upper(skel->data[s]) == name[n])) {
      ++s;
      ++n;
    } else if (star != SIZE_MAX) {
      s = star; /* the last `*` takes one more character */
      n = ++resume;
    } else {
      return 0;
    }
  }
  while (s < skel->len && skel->data[s] == '*') ++s;
  return s == skel->len;
}

void xb_memvar_clear(xb_ctx *ctx) {
  for (uint32_t i = 0; i < ctx->privateLen; ++i) {
    ctx->privates[i].id = CLEARED;
    xb_set_nil(&ctx->privates[i].hidden);
  }
  for (uint32_t i = 0; i < ctx->memvarCap; ++i) xb_set_nil(&ctx->memvars[i]);
}

/* __MemRelease( cName, ... ): RELEASE name, ...: the PRIVATE visible by each
 * name holds NIL; a PUBLIC no longer exists. A name with no variable is
 * skipped. */
void xbf___MEMRELEASE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  for (uint32_t k = 0; k < argc && !(ctx->pending & XB_PENDING_UNWIND); ++k) {
    uint32_t id;
    if (argv[k].type != XB_T_STRING || !xb_names_find_upper(&names, argv[k].v.s->data, argv[k].v.s->len, &id))
      continue;
    xb_item *b = binding(ctx, id);
    if (b->type != XB_T_CELL) continue;
    uint32_t i = ctx->privateLen;
    while (i > 0 && ctx->privates[i - 1].id != id) --i;
    if (i > 0) private_to_nil(ctx, i - 1); /* the PRIVATE of that name created last is the one visible */
    else xb_set_nil(b);
  }
}

/* __MemReleaseAll( cSkeleton, lLike ): RELEASE ALL [LIKE | EXCEPT]: the
 * PRIVATEs created by the function (or codeblock) calling it whose names
 * match the skeleton (with lLike), or do not (without it), hold NIL. */
void xbf___MEMRELEASEALL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (argc < 2 || argv[0].type != XB_T_STRING || argv[1].type != XB_T_LOGICAL) return;
  const xb_frame *caller = ctx->frames;
  for (uint32_t i = 0; i < ctx->privateLen && !(ctx->pending & XB_PENDING_UNWIND); ++i) {
    const xb_private *p = &ctx->privates[i];
    if (p->id == CLEARED || p->owner != caller) continue;
    size_t len;
    const char *name = xb_memvar_name(p->id, &len);
    if (xb_memvar_like(argv[0].v.s, name, len) == (argv[1].v.l != 0)) private_to_nil(ctx, i);
  }
}

/* __MemClear(): CLEAR MEMORY. */
void xbf___MEMCLEAR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_memvar_clear(ctx);
}
