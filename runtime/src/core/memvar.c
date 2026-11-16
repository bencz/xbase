/* Memory variables: PRIVATE and PUBLIC, found by name at run time (see the
 * xb_mv_* functions in xbrt.h). Names get numbers when modules register, and
 * when RESTORE meets a name no module uses, while the program runs: so
 * ctx->env->memvars may move whenever xBase code runs (an error block). Each
 * environment keeps, by number, the PRIVATE binding visible now (or one its
 * thread's creator had visible), and the PRIVATEs alive with the bindings they
 * hid; the PUBLICs are the program's, in one table every thread reads. A
 * name with no binding in the environment is the PUBLIC's, if there is one.
 * A binding is a cell (an XB_T_CELL item): what an argument @name passes, and
 * what PARAMETERS shares with an argument passed by reference (a cell holding
 * an element's reference reaches the element). */
#include "xbrt_int.h"

#include "core/shared.h"

#include <stdlib.h>
#include <string.h>

/* The names numbered so far, shared by every thread: found by name under the read lock, a name added under the write
 * lock (macros and RESTORE may number new names at any time). A name, once numbered, never changes: its spelling is
 * read by number without a lock, from chunks of SPELLED_CHUNK entries that are never moved or freed, a chunk's address
 * published (release) before any number in it is handed out, the count after the entry is written. */
#define SPELLED_CHUNK 1024u
#define SPELLED_CHUNKS 4096u /* names a program may number: SPELLED_CHUNK * SPELLED_CHUNKS */

typedef struct spelling {
  const char *name;
  size_t len;
} spelling;

static xb_names names;
static spelling *spelled[SPELLED_CHUNKS];
static uint32_t spelledCount;
static xb_rwlock namesLock;
static xb_once_flag namesLockMade = XB_ONCE_INIT;

static void make_names_lock(void) {
  if (!xb_rwlock_init(&namesLock)) xb_gc_fatal("cannot create the memory variables' lock");
}

static xb_rwlock *names_lock(void) {
  xb_once(&namesLockMade, make_names_lock);
  return &namesLock;
}

/* The PUBLIC variables, by number: chunks of SPELLED_CHUNK items, made when a PUBLIC first needs one and never moved
 * or freed, roots of the collector. An item is NIL or the variable's cell (shared once the program's storage is): a
 * PUBLIC is created under the names' write lock, its cell stored before its type (release), read without a lock, its
 * type first (acquire), and released by its type made NIL. */
static xb_item *publics[SPELLED_CHUNKS];

/* The number of a name (any letter case), if it has one. */
static int find_name(const char *name, size_t len, uint32_t *id) {
  xb_rwlock_read_lock(names_lock());
  int found = xb_names_find_upper(&names, name, len, id);
  xb_rwlock_read_unlock(names_lock());
  return found;
}

/* xb_private.id of a PRIVATE that CLEAR MEMORY released: it no longer names a
 * variable, and restores nothing when its activation ends. */
#define CLEARED UINT32_MAX

/* The number of `name` (upper case, kept by reference), new when it had none; under the write lock. */
static uint32_t number_locked(const char *name, size_t len) {
  uint32_t before = names.count;
  if (before == SPELLED_CHUNK * SPELLED_CHUNKS) xb_gc_fatal("too many memory variable names");
  uint32_t id = xb_names_add(&names, name, len);
  if (names.count == before) return id;
  spelling *chunk = spelled[id / SPELLED_CHUNK];
  if (!chunk) {
    chunk = malloc(SPELLED_CHUNK * sizeof *chunk);
    if (!chunk) xb_gc_fatal("out of memory");
    xb_atomic_storeptr((void **)&spelled[id / SPELLED_CHUNK], chunk, XB_RELEASE);
  }
  chunk[id % SPELLED_CHUNK] = (spelling){name, len};
  xb_atomic_store32(&spelledCount, names.count, XB_RELEASE);
  return id;
}

static uint32_t number(const char *name, size_t len) {
  xb_rwlock_write_lock(names_lock());
  uint32_t id = number_locked(name, len);
  xb_rwlock_write_unlock(names_lock());
  return id;
}

void xb_memvars_add(xb_memvar *memvars, uint32_t n) {
  for (uint32_t k = 0; k < n; ++k) memvars[k].id = number(memvars[k].name, strlen(memvars[k].name));
}

uint32_t xb_memvar_count(void) { return xb_atomic_load32(&spelledCount, XB_ACQUIRE); }

const char *xb_memvar_name(uint32_t id, size_t *len) {
  const spelling *chunk = xb_atomic_loadptr((void *const *)&spelled[id / SPELLED_CHUNK], XB_ACQUIRE);
  *len = chunk[id % SPELLED_CHUNK].len;
  return chunk[id % SPELLED_CHUNK].name; /* names live as long as the program */
}

/* The cell an item of the PUBLICs' table holds, NULL for none: its type first, then the cell. */
static void *public_cell(xb_item *p) {
  return xb_atomic_load32(&p->type, XB_ACQUIRE) == XB_T_CELL ? xb_atomic_loadptr(&p->v.p, XB_RELAXED) : NULL;
}

/* The PUBLIC numbered `id` into `*out` (its cell): 0 when there is none. */
static int public_get(uint32_t id, xb_item *out) {
  xb_item *chunk = xb_atomic_loadptr((void *const *)&publics[id / SPELLED_CHUNK], XB_ACQUIRE);
  void *cell = chunk ? public_cell(&chunk[id % SPELLED_CHUNK]) : NULL;
  if (cell) xb_set_ptr(out, XB_T_CELL, cell);
  return cell != NULL;
}

void xb_memvar_publics_each(void (*fn)(xb_item *it, void *data), void *data) {
  uint32_t chunks = (xb_memvar_count() + SPELLED_CHUNK - 1) / SPELLED_CHUNK;
  for (uint32_t c = 0; c < chunks; ++c) {
    xb_item *chunk = xb_atomic_loadptr((void *const *)&publics[c], XB_ACQUIRE);
    if (chunk)
      for (uint32_t i = 0; i < SPELLED_CHUNK; ++i) fn(&chunk[i], data);
  }
}

/* The environment's binding of variable `id`: NIL for none. */
static xb_item *binding(xb_ctx *ctx, uint32_t id) {
  if (id >= ctx->env->memvarCap) {
    uint32_t cap = xb_memvar_count(); /* every name numbered so far */
    xb_item *mv = realloc(ctx->env->memvars, cap * sizeof *mv);
    if (!mv) xb_gc_fatal("out of memory");
    for (uint32_t i = ctx->env->memvarCap; i < cap; ++i) xb_set_nil(&mv[i]);
    ctx->env->memvars = mv;
    ctx->env->memvarCap = cap;
  }
  return &ctx->env->memvars[id];
}

/* The binding visible for variable `id` into `*out`: the environment's, else the PUBLIC's; 0 when there is none. */
static int visible_binding(xb_ctx *ctx, uint32_t id, xb_item *out) {
  const xb_item *b = binding(ctx, id);
  if (b->type == XB_T_CELL) {
    *out = *b;
    return 1;
  }
  return public_get(id, out);
}

/* A new binding of its own, holding `value`, stored in `*r`. */
static void new_binding(xb_ctx *ctx, xb_item *r, const xb_item *value) {
  xb_gc_root_push(ctx, (xb_item *)value);
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell));
  xb_gc_root_pop(ctx, 1);
  c->value = *value; /* young: no barrier */
  xb_set_ptr(r, XB_T_CELL, c);
}

/* The frame of the activation running: a macro's evaluation is not one (see
 * XB_PROC_TRANSPARENT). */
static const xb_frame *activation(const xb_ctx *ctx) {
  const xb_frame *f = ctx->frames;
  while (f && f->proc && (f->proc->flags & XB_PROC_TRANSPARENT)) f = f->prev;
  return f;
}

/* A new binding for `id`, holding `value` (or the cell of a variable passed by
 * reference), recorded as a PRIVATE of the activation that runs now. */
static void push_private(xb_ctx *ctx, uint32_t id, const xb_item *value) {
  if (ctx->env->privateLen == ctx->env->privateCap) {
    uint32_t cap = ctx->env->privateCap ? ctx->env->privateCap * 2 : 32;
    xb_private *p = cap > ctx->env->privateCap ? realloc(ctx->env->privates, cap * sizeof *p) : NULL;
    if (!p) xb_gc_fatal("out of memory");
    ctx->env->privates = p;
    ctx->env->privateCap = cap;
  }
  int borrowed = value->type == XB_T_CELL; /* PARAMETERS: the variable passed */
  xb_item fresh;
  if (borrowed) fresh = *value;
  else new_binding(ctx, &fresh, value);
  xb_item *b = binding(ctx, id);
  xb_private *p = &ctx->env->privates[ctx->env->privateLen++];
  p->id = id;
  p->borrowed = (uint32_t)borrowed;
  p->owner = activation(ctx);
  p->hidden = *b;
  *b = fresh;
}

/* Whether the activation whose mark is `mark` created a PRIVATE `id`. */
static int activation_has(const xb_ctx *ctx, uint32_t id, uint32_t mark) {
  for (uint32_t i = ctx->env->privateLen; i-- > mark;)
    if (ctx->env->privates[i].id == id) return 1;
  return 0;
}

uint32_t xb_mv_mark(xb_ctx *ctx) { return ctx->env->privateLen; }

void xb_mv_release(xb_ctx *ctx, uint32_t mark) {
  while (ctx->env->privateLen > mark) {
    xb_private *p = &ctx->env->privates[--ctx->env->privateLen];
    if (p->id != CLEARED) ctx->env->memvars[p->id] = p->hidden;
  }
}

/* The binding visible for `m` into `*b`; with none, error 1003, which the
 * block may retry: 0 when it does not. */
static int visible(xb_ctx *ctx, const xb_memvar *m, xb_item *b) {
  for (uint16_t tries = 1;; ++tries) {
    if (visible_binding(ctx, m->id, b)) return 1;
    if (!xb_error_retry(ctx, EG_NOVAR, 1003, m->name, tries)) return 0;
  }
}

void xb_mv_get(xb_ctx *ctx, xb_item *r, const xb_memvar *m) {
  xb_item binding; /* a copy, rooted: the table may move, or the variable go, while a lock is waited for */
  if (!visible(ctx, m, &binding)) return;
  xb_gc_root_push(ctx, &binding);
  xb_ref_get(ctx, &binding, r);
  xb_gc_root_pop(ctx, 1);
}

void xb_mv_ref(xb_ctx *ctx, xb_item *r, const xb_memvar *m) {
  xb_item b;
  if (visible(ctx, m, &b)) *r = b;
}

/* Variable `id` takes `v`: the one visible, else a new PRIVATE. The binding
 * is copied and rooted: reaching an element may run the error block, which
 * may create variables (moving ctx->env->memvars) or release this one. */
static void assign(xb_ctx *ctx, uint32_t id, const xb_item *v) {
  xb_item b;
  if (!visible_binding(ctx, id, &b)) {
    push_private(ctx, id, v);
    return;
  }
  xb_gc_root_push(ctx, &b);
  xb_ref_set(ctx, &b, v);
  xb_gc_root_pop(ctx, 1);
}

void xb_mv_set(xb_ctx *ctx, const xb_memvar *m, const xb_item *v) { assign(ctx, m->id, v); }

void xb_mv_private(xb_ctx *ctx, const xb_memvar *m, uint32_t mark) {
  if (activation_has(ctx, m->id, mark)) return;
  xb_item nil;
  xb_set_nil(&nil);
  push_private(ctx, m->id, &nil);
}

/* PUBLIC m: a new variable of the program, unless one of that name is visible. Its cell is made (and shared, when the
 * program's storage is) before the lock is taken: nothing under it allocates or waits for a collection. */
void xb_mv_public(xb_ctx *ctx, const xb_memvar *m) {
  xb_item cell;
  if (visible_binding(ctx, m->id, &cell)) return;
  xb_item value;
  xb_set_logical(&value, strcmp(m->name, "CLIPPER") == 0); /* Clipper's rule: PUBLIC Clipper is .T. */
  new_binding(ctx, &cell, &value);
  if (xb_storage_is_shared()) xb_share(&cell);
  uint32_t c = m->id / SPELLED_CHUNK;
  xb_rwlock_write_lock(names_lock());
  if (!publics[c]) {
    xb_item *chunk = malloc(SPELLED_CHUNK * sizeof *chunk);
    if (!chunk) xb_gc_fatal("out of memory");
    for (uint32_t i = 0; i < SPELLED_CHUNK; ++i) xb_set_nil(&chunk[i]);
    xb_atomic_storeptr((void **)&publics[c], chunk, XB_RELEASE);
  }
  xb_item *p = &publics[c][m->id % SPELLED_CHUNK];
  if (!public_cell(p)) { /* another thread may have made it meanwhile: that one stays */
    xb_atomic_storeptr(&p->v.p, cell.v.p, XB_RELAXED);
    xb_atomic_store32(&p->type, XB_T_CELL, XB_RELEASE);
  }
  xb_rwlock_write_unlock(names_lock());
}

/* PUBLIC `id` no longer exists. */
static void public_release(uint32_t id) {
  xb_item *chunk = xb_atomic_loadptr((void *const *)&publics[id / SPELLED_CHUNK], XB_ACQUIRE);
  if (chunk) xb_atomic_store32(&chunk[id % SPELLED_CHUNK].type, XB_T_NIL, XB_RELEASE);
}

void xb_mv_param(xb_ctx *ctx, const xb_memvar *m, const xb_item *arg) { push_private(ctx, m->id, arg); }

int xb_memvar_value(xb_ctx *ctx, uint32_t id, xb_item *out) {
  xb_item b;
  if (!visible_binding(ctx, id, &b)) return 0;
  xb_gc_root_push(ctx, &b); /* as in xb_mv_get */
  xb_ref_get(ctx, &b, out);
  xb_gc_root_pop(ctx, 1);
  return 1;
}

uint32_t xb_memvar_number(const char *name, size_t len) {
  uint32_t id;
  if (find_name(name, len, &id)) return id;
  xb_rwlock_write_lock(names_lock());
  if (!xb_names_find_upper(&names, name, len, &id)) { /* another thread may have numbered it meanwhile */
    char *upper = malloc(len + 1); /* a name no module uses: kept for the program's life */
    if (!upper) xb_gc_fatal("out of memory");
    for (size_t i = 0; i < len; ++i) upper[i] = xb_ascii_upper(name[i]);
    upper[len] = 0;
    id = number_locked(upper, len);
  }
  xb_rwlock_write_unlock(names_lock());
  return id;
}

int xb_memvar_exists(xb_ctx *ctx, uint32_t id) {
  xb_item b;
  return visible_binding(ctx, id, &b);
}

int xb_memvar_peek(xb_ctx *ctx, const char *name, size_t len, xb_item *out) {
  uint32_t id;
  xb_item b;
  if (!find_name(name, len, &id) || !visible_binding(ctx, id, &b)) return 0;
  xb_cell_get(b.v.p, out); /* whole: the cell may be shared */
  return out->type != XB_T_REF; /* an element passed to PARAMETERS: read only with its checks */
}

void xb_memvar_assign(xb_ctx *ctx, const char *name, size_t len, const xb_item *value) {
  assign(ctx, xb_memvar_number(name, len), value);
}

typedef struct cell_search {
  const void *cell;
  int found;
} cell_search;

static void find_public_cell(xb_item *it, void *data) {
  cell_search *s = data;
  if (public_cell(it) == s->cell) s->found = 1;
}

/* Whether cell `c` is a memory variable's binding (visible, or hidden by a
 * PRIVATE, or a PUBLIC) anywhere but at `self`. */
static int memvar_cell(const xb_ctx *ctx, const void *c, const xb_item *self) {
  for (uint32_t i = 0; i < ctx->env->memvarCap; ++i) {
    const xb_item *b = &ctx->env->memvars[i];
    if (b != self && b->type == XB_T_CELL && b->v.p == c) return 1;
  }
  for (uint32_t i = 0; i < ctx->env->privateLen; ++i) {
    const xb_item *b = &ctx->env->privates[i].hidden;
    if (b != self && b->type == XB_T_CELL && b->v.p == c) return 1;
  }
  cell_search s = {c, 0};
  xb_memvar_publics_each(find_public_cell, &s);
  return s.found;
}

/* The variable of binding `b` holds NIL where it is (what designates it sees NIL). The binding is copied and rooted, as
 * in xb_mv_get. */
static void cell_to_nil(xb_ctx *ctx, const xb_item *b) {
  xb_item cell = *b, nil;
  xb_set_nil(&nil);
  xb_gc_root_push(ctx, &cell);
  xb_ref_set(ctx, &cell, &nil);
  xb_gc_root_pop(ctx, 1);
}

/* PRIVATE `i` holds NIL. A PARAMETERS one that received a LOCAL, a STATIC or
 * an element by reference lets it go for a variable of its own; any other is
 * a variable that becomes NIL where it is (a memory variable passed to
 * PARAMETERS by reference, and what was passed @name, see NIL too). */
static void private_to_nil(xb_ctx *ctx, uint32_t i) {
  xb_private *p = &ctx->env->privates[i];
  xb_item *b = NULL; /* where its binding is: in a later PRIVATE that hides it, else visible */
  for (uint32_t j = i + 1; j < ctx->env->privateLen && !b; ++j)
    if (ctx->env->privates[j].id == p->id) b = &ctx->env->privates[j].hidden;
  if (!b) b = binding(ctx, p->id);
  if (p->borrowed && !memvar_cell(ctx, b->v.p, b)) {
    xb_item nil;
    xb_set_nil(&nil);
    new_binding(ctx, b, &nil);
    p->borrowed = 0;
  } else {
    cell_to_nil(ctx, b);
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
  for (uint32_t i = 0; i < ctx->env->privateLen; ++i) {
    ctx->env->privates[i].id = CLEARED;
    xb_set_nil(&ctx->env->privates[i].hidden);
  }
  for (uint32_t i = 0; i < ctx->env->memvarCap; ++i) xb_set_nil(&ctx->env->memvars[i]);
  for (uint32_t id = 0, n = xb_memvar_count(); id < n; ++id) public_release(id);
}

/* __MemRelease( cName, ... ): RELEASE name, ...: the PRIVATE visible by each
 * name holds NIL (one the thread's creator had visible too); a PUBLIC no
 * longer exists. A name with no variable is skipped. */
void xbf___MEMRELEASE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  for (uint32_t k = 0; k < argc && !(xb_pending_get(ctx) & XB_PENDING_UNWIND); ++k) {
    uint32_t id;
    if (argv[k].type != XB_T_STRING || !find_name(argv[k].v.s->data, argv[k].v.s->len, &id))
      continue;
    xb_item *b = binding(ctx, id);
    if (b->type != XB_T_CELL) {
      public_release(id);
      continue;
    }
    uint32_t i = ctx->env->privateLen;
    while (i > 0 && ctx->env->privates[i - 1].id != id) --i;
    if (i > 0) private_to_nil(ctx, i - 1); /* the PRIVATE of that name created last is the one visible */
    else cell_to_nil(ctx, b);              /* inherited from the thread's creator: the same variable */
  }
}

/* __MemReleaseAll( cSkeleton, lLike ): RELEASE ALL [LIKE | EXCEPT]: the
 * PRIVATEs created by the function (or codeblock) calling it whose names
 * match the skeleton (with lLike), or do not (without it), hold NIL. */
void xbf___MEMRELEASEALL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  if (argc < 2 || argv[0].type != XB_T_STRING || argv[1].type != XB_T_LOGICAL) return;
  const xb_frame *caller = activation(ctx);
  for (uint32_t i = 0; i < ctx->env->privateLen && !(xb_pending_get(ctx) & XB_PENDING_UNWIND); ++i) {
    const xb_private *p = &ctx->env->privates[i];
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

/* The block MemVarBlock() makes: its cell holds the variable's number (an
 * integer item). Called with an argument that is not NIL, it assigns it. */
static void memvar_block(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, xb_block *self) {
  uint32_t id = (uint32_t)self->cells[0]->value.v.i;
  size_t len;
  xb_memvar m = {xb_memvar_name(id, &len), id};
  if (argc && argv[0].type != XB_T_NIL) {
    xb_mv_set(ctx, &m, &argv[0]);
    *ret = argv[0];
    return;
  }
  xb_mv_get(ctx, ret, &m);
}

/* MemVarBlock( cName ): a block that reads the memory variable cName (any
 * letter case) and, called with a value, assigns it; NIL when no variable of
 * that name is visible. */
void xbf_MEMVARBLOCK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  uint32_t id;
  if (!argc || argv[0].type != XB_T_STRING) return;
  if (!find_name(argv[0].v.s->data, argv[0].v.s->len, &id) || !xb_memvar_exists(ctx, id)) return;
  xb_block *b = xb_block_new(ctx, ret, memvar_block, 1);
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell)); /* the block is rooted in `ret` */
  xb_set_int(&c->value, id);
  b->cells[0] = c;
  xb_gc_write(b);
}

void xb_memvar_share(xb_ctx *ctx) {
  for (xb_env *env = ctx->env; env; env = env->outer) {
    for (uint32_t i = 0; i < env->memvarCap; ++i) xb_share(&env->memvars[i]);
    for (uint32_t i = 0; i < env->privateLen; ++i) xb_share(&env->privates[i].hidden);
  }
}

static void share_public(xb_item *it, void *data) {
  XB_UNUSED(data);
  xb_share(it);
}

void xb_memvar_share_publics(void) { xb_memvar_publics_each(share_public, NULL); }

void xb_memvar_capture(xb_ctx *ctx, xb_item *r) {
  uint32_t n = 0;
  for (uint32_t i = 0; i < ctx->env->memvarCap; ++i) n += ctx->env->memvars[i].type == XB_T_CELL;
  if (!n) {
    xb_set_nil(r);
    return;
  }
  xb_array *a = xb_array_alloc(2 * (uint64_t)n); /* `a`, on the stack, stays where it is; the cells are rooted */
  xb_set_array(r, a);
  uint32_t k = 0;
  for (uint32_t i = 0; i < ctx->env->memvarCap && k < 2 * n; ++i) {
    if (ctx->env->memvars[i].type != XB_T_CELL) continue;
    xb_set_int(&a->items[k++], i);
    a->items[k++] = ctx->env->memvars[i];
  }
  xb_gc_write(a);
}

void xb_memvar_restore(xb_ctx *ctx, const xb_item *pairs) {
  if (pairs->type != XB_T_ARRAY) return;
  size_t n = xb_array_length(pairs->v.p);
  for (size_t k = 0; k + 1 < n; k += 2) {
    xb_item id;
    xb_array_get(pairs->v.p, k, &id);
    xb_item *b = binding(ctx, (uint32_t)id.v.i); /* may grow the table: the address after it */
    xb_array_get(pairs->v.p, k + 1, b);
  }
}
