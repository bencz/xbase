/* Memory variables: PRIVATE and PUBLIC, found by name at run time (the xb_mv_* functions of xbrt.h).
 *
 * Names. A variable's name has a number. The names of the modules (their MEMVAR, PRIVATE, PUBLIC and PARAMETERS
 * statements) get theirs when the modules register and keep them: permanent. A name met only while the program runs
 * (a macro, RESTORE, MemVarBlock(), PRIVATE &cName) has one while something holds it: a binding of an environment, a
 * PRIVATE alive, the PUBLIC of that name, a snapshot of an environment (which keeps the name's text instead), a
 * function using it a moment (xb_memvar_hold). When the last holder lets go, the name and its number are freed and the
 * number goes to another name later, so a program that makes names without end keeps only those in use. Compiled code
 * and the macro compiler's code keep the numbers of permanent names only; a macro keeps the text of any other name and
 * holds its number while one of its operations runs.
 *
 * Bindings. Each environment keeps, by number, the PRIVATE binding visible now (or one its creator had visible): a
 * small hash of the numbers bound (open addressing, linear probing, at most half full), which environments that
 * inherit it share until one of them changes it (copy on write: an asynchronous call copies nothing, and an
 * environment costs what it binds, never the number of names). A permanent name's entry stays when its PRIVATE ends,
 * NIL, so that a function that makes one at every call finds it in place (bounded: the program's names); another
 * name's goes, and lets the name go. A binding is a cell (an XB_T_CELL item): what an
 * argument @name passes, and what PARAMETERS shares with an argument passed by reference (a cell holding an element's
 * reference reaches the element). The PRIVATEs alive, each with the binding it hid, are a stack of the environment.
 * The PUBLICs are the program's, in one table by number that every thread reads. A name with no binding in the
 * environment is the PUBLIC's, if there is one. */
#include "xbrt_int.h"

#include "core/shared.h"

#include <stdlib.h>
#include <string.h>

/* The names, shared by every thread: found by name under the read lock, numbered and freed under the write lock. A
 * name's spelling is read by number without a lock by whoever holds it (or for a permanent one), from chunks of
 * NAME_CHUNK entries that are never moved or freed, a chunk's address published (release) before any number in it is
 * handed out. Numbers in use at once: NAME_CHUNK * NAME_CHUNKS (64 M: more names than memory holds). */
#define NAME_CHUNK 1024u
#define NAME_CHUNKS 65536u

typedef struct spelling {
  const char *name;   /* NULL: the number is free */
  size_t len;
  uint32_t holders;   /* of a name that is not permanent (atomic) */
  uint32_t permanent; /* a module's: never freed */
} spelling;

static xb_names names;
static spelling *spelled[NAME_CHUNKS];
static uint32_t numbered;                 /* numbers handed out so far: 0 .. numbered - 1, some free (atomic) */
static uint32_t *freeNumbers;             /* numbers freed, taken again first */
static uint32_t freeLen, freeCap;
static xb_rwlock namesLock;
static xb_once_flag namesLockMade = XB_ONCE_INIT;

static void make_names_lock(void) {
  if (!xb_rwlock_init(&namesLock)) xb_gc_fatal("cannot create the memory variables' lock");
}

static xb_rwlock *names_lock(void) {
  xb_once(&namesLockMade, make_names_lock);
  return &namesLock;
}

static spelling *spelling_of(uint32_t id) {
  spelling *chunk = xb_atomic_loadptr((void *const *)&spelled[id / NAME_CHUNK], XB_ACQUIRE);
  return &chunk[id % NAME_CHUNK];
}

#define NO_ID UINT32_MAX /* no number: an empty entry of a table */

/* The PUBLIC variables, by number: chunks of NAME_CHUNK items, made when a PUBLIC first needs one and never moved
 * or freed, roots of the collector. An item is NIL or the variable's cell (shared once the program's storage is): a
 * PUBLIC is created under the names' write lock, its cell stored before its type (release), read without a lock, its
 * type first (acquire), and released by its type made NIL (an exchange: one release lets the name go). It holds its
 * name. */
static xb_item *publics[NAME_CHUNKS];

/* A number for `name` (upper case, kept by reference: malloc'd unless `permanent`), new: under the write lock. */
static uint32_t number_new(const char *name, size_t len, int permanent) {
  uint32_t id;
  if (freeLen) {
    id = freeNumbers[--freeLen];
  } else {
    id = numbered;
    if (id == NAME_CHUNK * NAME_CHUNKS) xb_gc_fatal("too many memory variable names in use");
    if (!spelled[id / NAME_CHUNK]) {
      spelling *chunk = calloc(NAME_CHUNK, sizeof *chunk);
      if (!chunk) xb_gc_fatal("out of memory");
      xb_atomic_storeptr((void **)&spelled[id / NAME_CHUNK], chunk, XB_RELEASE);
    }
    xb_atomic_store32(&numbered, id + 1, XB_RELEASE);
  }
  spelling *s = spelling_of(id);
  s->name = name;
  s->len = len;
  s->permanent = (uint32_t)permanent;
  xb_atomic_store32(&s->holders, permanent ? 0 : 1, XB_RELAXED);
  xb_names_insert(&names, name, len, id);
  return id;
}

/* The name's spelling, upper case, in memory of its own. */
static char *upper_copy(const char *name, size_t len) {
  char *upper = malloc(len + 1);
  if (!upper) xb_gc_fatal("out of memory");
  for (size_t i = 0; i < len; ++i) upper[i] = xb_ascii_upper(name[i]);
  upper[len] = 0;
  return upper;
}

void xb_memvars_add(xb_memvar *memvars, uint32_t n) {
  xb_rwlock_write_lock(names_lock());
  for (uint32_t k = 0; k < n; ++k) {
    const char *name = memvars[k].name;
    size_t len = strlen(name);
    uint32_t id;
    if (xb_names_find(&names, name, len, &id)) xb_atomic_store32(&spelling_of(id)->permanent, 1, XB_RELAXED);
    else id = number_new(name, len, 1);
    memvars[k].id = id;
  }
  xb_rwlock_write_unlock(names_lock());
}

/* One more holder of name `id` (in use, or permanent): whether it is permanent (which no hold changes). */
static int hold(uint32_t id) {
  spelling *s = spelling_of(id);
  if (xb_atomic_load32(&s->permanent, XB_RELAXED)) return 1;
  xb_atomic_add32(&s->holders, 1, XB_RELAXED);
  return 0;
}

void xb_memvar_unhold(uint32_t id) {
  spelling *s = spelling_of(id);
  if (xb_atomic_load32(&s->permanent, XB_RELAXED) || xb_atomic_add32(&s->holders, (uint32_t)-1, XB_ACQ_REL) != 1)
    return;
  xb_rwlock_write_lock(names_lock()); /* the last holder: freed, unless it was held again or made permanent meanwhile */
  if (s->name && !s->permanent && xb_atomic_load32(&s->holders, XB_ACQUIRE) == 0) {
    xb_names_remove(&names, s->name, s->len);
    free((char *)s->name);
    s->name = NULL;
    if (freeLen == freeCap) {
      uint32_t cap = freeCap ? freeCap * 2 : 64;
      uint32_t *grown = realloc(freeNumbers, cap * sizeof *grown);
      if (!grown) xb_gc_fatal("out of memory");
      freeNumbers = grown;
      freeCap = cap;
    }
    freeNumbers[freeLen++] = id;
  }
  xb_rwlock_write_unlock(names_lock());
}

int xb_memvar_find_hold(const char *name, size_t len, uint32_t *id) {
  xb_rwlock_read_lock(names_lock());
  int found = xb_names_find_upper(&names, name, len, id);
  if (found) hold(*id); /* under the read lock: no one frees it meanwhile */
  xb_rwlock_read_unlock(names_lock());
  return found;
}

uint32_t xb_memvar_hold(const char *name, size_t len) {
  uint32_t id;
  if (xb_memvar_find_hold(name, len, &id)) return id;
  char *upper = upper_copy(name, len);
  xb_rwlock_write_lock(names_lock());
  if (xb_names_find(&names, upper, len, &id)) { /* another thread numbered it meanwhile */
    hold(id);
    free(upper);
  } else {
    id = number_new(upper, len, 0);
  }
  xb_rwlock_write_unlock(names_lock());
  return id;
}

/* The context's cache of names (xb_ctx.nameIds): the entry of a name, by a hash of its letters in upper case. */
static uint32_t name_slot(const char *name, size_t len) {
  uint32_t h = UINT32_C(2166136261);
  for (size_t i = 0; i < len; ++i) h = (h ^ (unsigned char)xb_ascii_upper(name[i])) * UINT32_C(16777619);
  return h % XB_NAME_CACHE;
}

/* Whether the name numbered `id` (held) is `name` in any letter case. */
static int spelled_as(uint32_t id, const char *name, size_t len) {
  const spelling *s = spelling_of(id);
  if (s->len != len) return 0;
  for (size_t i = 0; i < len; ++i)
    if (s->name[i] != xb_ascii_upper(name[i])) return 0;
  return 1;
}

void xb_memvar_cache_drop(xb_ctx *ctx) {
  for (uint32_t k = 0; k < XB_NAME_CACHE; ++k)
    if (ctx->nameIds[k] != NO_ID && !ctx->namePins[k]) {
      xb_memvar_unhold(ctx->nameIds[k]);
      ctx->nameIds[k] = NO_ID;
    }
}

void xb_memvar_let_go(xb_ctx *ctx) {
  xb_memvar_bindings_free(ctx->env);
  xb_memvar_cache_drop(ctx);
}

uint32_t xb_memvar_use(xb_ctx *ctx, const char *name, size_t len, int number, uint32_t *slot) {
  uint32_t k = name_slot(name, len), id = ctx->nameIds[k];
  if (id != NO_ID && spelled_as(id, name, len)) { /* the usual case: no lock, no atomic */
    ++ctx->namePins[k];
    *slot = k;
    return id;
  }
  *slot = NO_ID;
  if (number) id = xb_memvar_hold(name, len);
  else if (!xb_memvar_find_hold(name, len, &id)) return NO_ID;
  if (ctx->namePins[k]) return id; /* the entry's name is in use: this one is held for the operation alone */
  if (ctx->nameIds[k] != NO_ID) xb_memvar_unhold(ctx->nameIds[k]);
  ctx->nameIds[k] = id; /* the cache takes the hold */
  ctx->namePins[k] = 1;
  *slot = k;
  return id;
}

void xb_memvar_done(xb_ctx *ctx, uint32_t id, uint32_t slot) {
  if (slot == NO_ID) xb_memvar_unhold(id);
  else --ctx->namePins[slot];
}

int xb_memvar_permanent(const char *name, size_t len, uint32_t *id) {
  xb_rwlock_read_lock(names_lock());
  int found = xb_names_find_upper(&names, name, len, id) && xb_atomic_load32(&spelling_of(*id)->permanent, XB_RELAXED);
  xb_rwlock_read_unlock(names_lock());
  return found;
}

const char *xb_memvar_name(uint32_t id, size_t *len) {
  const spelling *s = spelling_of(id);
  *len = s->len;
  return s->name;
}

/* The cell an item of the PUBLICs' table holds, NULL for none: its type first, then the cell. */
static void *public_cell(xb_item *p) {
  return xb_atomic_load32(&p->type, XB_ACQUIRE) == XB_T_CELL ? xb_atomic_loadptr(&p->v.p, XB_RELAXED) : NULL;
}

/* The PUBLIC numbered `id` into `*out` (its cell): 0 when there is none. */
static int public_get(uint32_t id, xb_item *out) {
  xb_item *chunk = xb_atomic_loadptr((void *const *)&publics[id / NAME_CHUNK], XB_ACQUIRE);
  void *cell = chunk ? public_cell(&chunk[id % NAME_CHUNK]) : NULL;
  if (cell) xb_set_ptr(out, XB_T_CELL, cell);
  return cell != NULL;
}

void xb_memvar_publics_each(void (*fn)(xb_item *it, void *data), void *data) {
  uint32_t chunks = (xb_atomic_load32(&numbered, XB_ACQUIRE) + NAME_CHUNK - 1) / NAME_CHUNK;
  for (uint32_t c = 0; c < chunks; ++c) {
    xb_item *chunk = xb_atomic_loadptr((void *const *)&publics[c], XB_ACQUIRE);
    if (chunk)
      for (uint32_t i = 0; i < NAME_CHUNK; ++i) fn(&chunk[i], data);
  }
}

/* An environment's bindings (see the top of the file). */

typedef struct bslot {
  uint32_t id; /* NO_ID: empty */
  xb_item cell;
} bslot;

struct xb_bindings {
  uint32_t users; /* environments that use it (atomic): changed in place only by its only user */
  uint32_t count, mask;
  bslot slots[];
};

static uint32_t bindings_home(const xb_bindings *b, uint32_t id) {
  uint32_t h = id * UINT32_C(0x9E3779B1);
  return (h ^ (h >> 15)) & b->mask;
}

static bslot *bindings_find(const xb_bindings *b, uint32_t id) {
  if (!b) return NULL;
  for (uint32_t i = bindings_home(b, id);; i = (i + 1) & b->mask) {
    const bslot *s = &b->slots[i];
    if (s->id == id) return (bslot *)s;
    if (s->id == NO_ID) return NULL;
  }
}

static xb_bindings *bindings_new(uint32_t cap) {
  xb_bindings *b = malloc(sizeof *b + cap * sizeof(bslot));
  if (!b) xb_gc_fatal("out of memory");
  b->users = 1;
  b->count = 0;
  b->mask = cap - 1;
  for (uint32_t i = 0; i < cap; ++i) b->slots[i].id = NO_ID;
  return b;
}

/* Puts slot `from` in `b`, which has room (rehashing: the names are held already). */
static void bindings_place(xb_bindings *b, const bslot *from) {
  uint32_t i = bindings_home(b, from->id);
  while (b->slots[i].id != NO_ID) i = (i + 1) & b->mask;
  b->slots[i] = *from;
  ++b->count;
}

/* A copy of `b` with room for `cap` slots (a power of two, more than twice its count), holding the names again. */
static xb_bindings *bindings_copy(const xb_bindings *b, uint32_t cap) {
  xb_bindings *c = bindings_new(cap);
  for (uint32_t i = 0; i <= b->mask; ++i)
    if (b->slots[i].id != NO_ID) {
      bindings_place(c, &b->slots[i]);
      hold(b->slots[i].id);
    }
  return c;
}

/* A user lets `b` go; the last one frees it, letting its names go. */
static void bindings_release(xb_bindings *b) {
  if (!b || xb_atomic_add32(&b->users, (uint32_t)-1, XB_ACQ_REL) != 1) return;
  for (uint32_t i = 0; i <= b->mask; ++i)
    if (b->slots[i].id != NO_ID) xb_memvar_unhold(b->slots[i].id);
  free(b);
}

/* The environment's entry for `id`, or NULL: the entry it found last first (a function's PRIVATE is read, assigned,
 * made and ended by number, again and again). */
static bslot *bindings_at(xb_env *e, uint32_t id) {
  xb_bindings *b = e->bindings;
  if (!b) return NULL;
  if (e->bindHint <= b->mask && b->slots[e->bindHint].id == id) return &b->slots[e->bindHint];
  bslot *s = bindings_find(b, id);
  if (s) e->bindHint = (uint32_t)(s - b->slots);
  return s;
}

/* The environment's bindings, its own (copied when it shares them), with room for one more. */
static xb_bindings *bindings_own(xb_env *e) {
  xb_bindings *b = e->bindings;
  if (!b) return e->bindings = bindings_new(8);
  int alone = !e->borrowsBindings && xb_atomic_load32(&b->users, XB_ACQUIRE) == 1;
  if (alone && 2 * (b->count + 1) <= b->mask + 1) return b;
  uint32_t cap = b->mask + 1;
  while (2 * (b->count + 1) > cap) cap *= 2;
  xb_bindings *c = bindings_copy(b, cap);
  if (e->borrowsBindings) e->borrowsBindings = 0;
  else bindings_release(b);
  return e->bindings = c;
}

/* The binding of `id` in the environment's own bindings (new and NIL when it had none): valid until they change. */
static xb_item *bindings_put(xb_env *e, uint32_t id) {
  xb_bindings *b = e->bindings;
  if (b && !e->borrowsBindings && xb_atomic_load32(&b->users, XB_ACQUIRE) == 1) { /* its own, the entry there: usual */
    bslot *s = bindings_at(e, id);
    if (s) return &s->cell;
  }
  b = bindings_own(e);
  bslot *s = bindings_find(b, id);
  if (s) return &s->cell;
  bslot fresh = {id, {0}};
  xb_set_nil(&fresh.cell);
  hold(id);
  uint32_t i = bindings_home(b, id);
  while (b->slots[i].id != NO_ID) i = (i + 1) & b->mask;
  b->slots[i] = fresh;
  ++b->count;
  return &b->slots[i].cell;
}

/* The environment binds `id` no more. */
static void bindings_remove(xb_env *e, uint32_t id) {
  if (!bindings_find(e->bindings, id)) return;
  xb_bindings *b = bindings_own(e);
  uint32_t i = (uint32_t)(bindings_find(b, id) - b->slots);
  for (uint32_t j = (i + 1) & b->mask; b->slots[j].id != NO_ID; j = (j + 1) & b->mask) {
    uint32_t home = bindings_home(b, b->slots[j].id);
    if (((j - home) & b->mask) >= ((j - i) & b->mask)) { /* its home is not between the hole and it */
      b->slots[i] = b->slots[j];
      i = j;
    }
  }
  b->slots[i].id = NO_ID;
  --b->count;
  xb_memvar_unhold(id);
}

void xb_memvar_bindings_each(xb_env *e, void (*fn)(xb_item *it, void *data), void *data) {
  const xb_bindings *b = e->bindings;
  if (b)
    for (uint32_t i = 0; i <= b->mask; ++i)
      if (b->slots[i].id != NO_ID) fn((xb_item *)&b->slots[i].cell, data);
}

void xb_memvar_bindings_free(xb_env *e) {
  if (!e->borrowsBindings) bindings_release(e->bindings); /* borrowed: its caller's, not released */
  e->borrowsBindings = 0;
  e->bindings = NULL;
}

/* The binding visible for variable `id` into `*out`: the environment's, else the PUBLIC's; 0 when there is none. */
static int visible_binding(xb_ctx *ctx, uint32_t id, xb_item *out) {
  const bslot *s = bindings_at(ctx->env, id);
  if (s && s->cell.type == XB_T_CELL) {
    *out = s->cell;
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
  xb_env *e = ctx->env;
  if (e->privateLen == e->privateCap) {
    uint32_t cap = e->privateCap ? e->privateCap * 2 : 32;
    xb_private *p = cap > e->privateCap ? realloc(e->privates, cap * sizeof *p) : NULL;
    if (!p) xb_gc_fatal("out of memory");
    e->privates = p;
    e->privateCap = cap;
  }
  int borrowed = value->type == XB_T_CELL; /* PARAMETERS: the variable passed */
  xb_item fresh;
  if (borrowed) fresh = *value;
  else new_binding(ctx, &fresh, value);
  xb_item *b = bindings_put(e, id);
  xb_private *p = &e->privates[e->privateLen++];
  p->permanent = (uint16_t)hold(id);
  p->id = id;
  p->borrowed = (uint16_t)borrowed;
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

/* The PRIVATEs' room is kept while a quarter of it or more is used, or it is this small; past that it halves, so that
 * the peak of a deep recursion does not stay. */
#define PRIVATES_KEEP 1024u

void xb_mv_release(xb_ctx *ctx, uint32_t mark) {
  xb_env *e = ctx->env;
  while (e->privateLen > mark) {
    xb_private *p = &e->privates[--e->privateLen];
    if (p->id == XB_PRIVATE_CLEARED) continue;
    if (p->hidden.type == XB_T_CELL || p->permanent) {
      *bindings_put(e, p->id) = p->hidden; /* a permanent name keeps its entry, NIL: the next PRIVATE finds it there */
    } else {
      bindings_remove(e, p->id);
    }
    if (!p->permanent) xb_memvar_unhold(p->id);
  }
  if (e->privateCap > PRIVATES_KEEP && e->privateLen <= e->privateCap / 4) {
    uint32_t cap = e->privateCap / 2;
    xb_private *p = realloc(e->privates, cap * sizeof *p);
    if (p) { /* else the room stays as it was */
      e->privates = p;
      e->privateCap = cap;
    }
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
  xb_item binding; /* a copy, rooted: the bindings may change, or the variable go, while a lock is waited for */
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
 * may create variables (changing the bindings) or release this one. */
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
  uint32_t c = m->id / NAME_CHUNK;
  xb_rwlock_write_lock(names_lock());
  if (!publics[c]) {
    xb_item *chunk = malloc(NAME_CHUNK * sizeof *chunk);
    if (!chunk) xb_gc_fatal("out of memory");
    for (uint32_t i = 0; i < NAME_CHUNK; ++i) xb_set_nil(&chunk[i]);
    xb_atomic_storeptr((void **)&publics[c], chunk, XB_RELEASE);
  }
  xb_item *p = &publics[c][m->id % NAME_CHUNK];
  if (!public_cell(p)) { /* another thread may have made it meanwhile: that one stays */
    hold(m->id);
    xb_atomic_storeptr(&p->v.p, cell.v.p, XB_RELAXED);
    xb_atomic_store32(&p->type, XB_T_CELL, XB_RELEASE);
  }
  xb_rwlock_write_unlock(names_lock());
}

/* PUBLIC `id` no longer exists. */
static void public_release(uint32_t id) {
  xb_item *chunk = xb_atomic_loadptr((void *const *)&publics[id / NAME_CHUNK], XB_ACQUIRE);
  if (chunk && xb_atomic_xchg32(&chunk[id % NAME_CHUNK].type, XB_T_NIL, XB_ACQ_REL) == XB_T_CELL)
    xb_memvar_unhold(id);
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

int xb_memvar_exists_named(xb_ctx *ctx, const char *name, size_t len) {
  uint32_t id;
  xb_item b;
  xb_rwlock_read_lock(names_lock()); /* the number stays the name's while it is used */
  int found = xb_names_find_upper(&names, name, len, &id) && visible_binding(ctx, id, &b);
  xb_rwlock_read_unlock(names_lock());
  return found;
}

int xb_memvar_peek(xb_ctx *ctx, const char *name, size_t len, xb_item *out) {
  uint32_t id;
  xb_item b;
  xb_rwlock_read_lock(names_lock());
  int found = xb_names_find_upper(&names, name, len, &id) && visible_binding(ctx, id, &b);
  if (found) xb_cell_get(b.v.p, out); /* whole: the cell may be shared */
  xb_rwlock_read_unlock(names_lock());
  return found && out->type != XB_T_REF; /* an element passed to PARAMETERS: read only with its checks */
}

void xb_memvar_assign(xb_ctx *ctx, const char *name, size_t len, const xb_item *value) {
  uint32_t slot, id = xb_memvar_use(ctx, name, len, 1, &slot);
  assign(ctx, id, value);
  xb_memvar_done(ctx, id, slot);
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
  const xb_bindings *bs = ctx->env->bindings;
  for (uint32_t i = 0; bs && i <= bs->mask; ++i) {
    const xb_item *b = &bs->slots[i].cell;
    if (bs->slots[i].id != NO_ID && b != self && b->type == XB_T_CELL && b->v.p == c) return 1;
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
  if (!b) b = bindings_put(ctx->env, p->id); /* its own: no other binding changes while it is used below */
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
    if (s < skel->len && xb_str_data(skel)[s] == '*') {
      star = ++s;
      resume = n;
    } else if (s < skel->len && (xb_str_data(skel)[s] == '?' || xb_ascii_upper(xb_str_data(skel)[s]) == name[n])) {
      ++s;
      ++n;
    } else if (star != SIZE_MAX) {
      s = star; /* the last `*` takes one more character */
      n = ++resume;
    } else {
      return 0;
    }
  }
  while (s < skel->len && xb_str_data(skel)[s] == '*') ++s;
  return s == skel->len;
}

void xb_memvar_clear(xb_ctx *ctx) {
  xb_env *e = ctx->env;
  for (uint32_t i = 0; i < e->privateLen; ++i) {
    if (e->privates[i].id != XB_PRIVATE_CLEARED) xb_memvar_unhold(e->privates[i].id);
    e->privates[i].id = XB_PRIVATE_CLEARED;
    xb_set_nil(&e->privates[i].hidden);
  }
  xb_memvar_bindings_free(e);
  for (uint32_t id = 0, n = xb_atomic_load32(&numbered, XB_ACQUIRE); id < n; ++id) public_release(id);
}

/* __MemRelease( cName, ... ): RELEASE name, ...: the PRIVATE visible by each
 * name holds NIL (one the thread's creator had visible too); a PUBLIC no
 * longer exists. A name with no variable is skipped. */
void xbf___MEMRELEASE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  for (uint32_t k = 0; k < argc && !(xb_pending_get(ctx) & XB_PENDING_UNWIND); ++k) {
    uint32_t id;
    if (argv[k].type != XB_T_STRING || !xb_memvar_find_hold(xb_str_data(argv[k].v.s), argv[k].v.s->len, &id)) continue;
    const bslot *b = bindings_find(ctx->env->bindings, id);
    if (!b || b->cell.type != XB_T_CELL) {
      public_release(id);
    } else {
      uint32_t i = ctx->env->privateLen;
      while (i > 0 && ctx->env->privates[i - 1].id != id) --i;
      if (i > 0) private_to_nil(ctx, i - 1); /* the PRIVATE of that name created last is the one visible */
      else cell_to_nil(ctx, &b->cell);       /* inherited from the thread's creator: the same variable */
    }
    xb_memvar_unhold(id);
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
    if (p->id == XB_PRIVATE_CLEARED || p->owner != caller) continue;
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

/* The block MemVarBlock() makes: its cell holds the variable's number (an integer item) when its name is permanent,
 * else its name (a string), found again each time. Called with an argument that is not NIL, it assigns it. */
static void memvar_block(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, xb_block *self) {
  const xb_item *named = &self->cells[0]->value;
  uint32_t slot = NO_ID, id = (uint32_t)named->v.i;
  if (named->type == XB_T_STRING) id = xb_memvar_use(ctx, xb_str_data(named->v.s), named->v.s->len, 1, &slot);
  size_t len;
  xb_memvar m = {xb_memvar_name(id, &len), id};
  if (argc && argv[0].type != XB_T_NIL) {
    xb_mv_set(ctx, &m, &argv[0]);
    *ret = argv[0];
  } else {
    xb_mv_get(ctx, ret, &m);
  }
  if (named->type == XB_T_STRING) xb_memvar_done(ctx, id, slot);
}

/* MemVarBlock( cName ): a block that reads the memory variable cName (any
 * letter case) and, called with a value, assigns it; NIL when no variable of
 * that name is visible. */
void xbf_MEMVARBLOCK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (!argc || argv[0].type != XB_T_STRING) return;
  if (!xb_memvar_exists_named(ctx, xb_str_data(argv[0].v.s), argv[0].v.s->len)) return;
  uint32_t id;
  int permanent = xb_memvar_permanent(xb_str_data(argv[0].v.s), argv[0].v.s->len, &id);
  xb_block *b = xb_block_new(ctx, ret, memvar_block, NULL, 0, 1);
  xb_cell *c = xb_gc_alloc(XB_GC_CELL, sizeof(xb_cell)); /* the block is rooted in `ret` */
  xb_set_nil(&c->value);
  b->cells[0] = c;
  xb_gc_write(b);
  if (permanent) {
    xb_set_int(&c->value, id);
  } else {
    char *upper = upper_copy(xb_str_data(argv[0].v.s), argv[0].v.s->len);
    xb_set_str(&c->value, xb_str_new(upper, argv[0].v.s->len));
    xb_gc_write(c);
    free(upper);
  }
}

static void share_item(xb_item *it, void *data) {
  XB_UNUSED(data);
  xb_share(it);
}

void xb_memvar_share(xb_ctx *ctx) {
  for (xb_env *env = ctx->env; env; env = env->outer) {
    xb_memvar_bindings_each(env, share_item, NULL);
    for (uint32_t i = 0; i < env->privateLen; ++i) xb_share(&env->privates[i].hidden);
  }
}

void xb_memvar_share_publics(void) { xb_memvar_publics_each(share_item, NULL); }

void xb_memvar_capture(xb_ctx *ctx, xb_item *r) {
  const xb_bindings *b = ctx->env->bindings;
  uint32_t n = 0;
  for (uint32_t i = 0; b && i <= b->mask; ++i) n += b->slots[i].id != NO_ID && b->slots[i].cell.type == XB_T_CELL;
  if (!n) {
    xb_set_nil(r);
    return;
  }
  xb_array *a = xb_array_alloc(2 * (uint64_t)n); /* `a`, on the stack, stays where it is; the cells are rooted */
  xb_set_array(r, a);
  uint32_t k = 0;
  for (uint32_t i = 0; i <= b->mask && k < 2 * n; ++i) {
    const bslot *s = &b->slots[i];
    if (s->id == NO_ID || s->cell.type != XB_T_CELL) continue;
    if (xb_atomic_load32(&spelling_of(s->id)->permanent, XB_RELAXED)) {
      xb_set_int(&xb_array_items(a)[k], s->id);
    } else { /* its text: the snapshot holds no number, which may go to another name once the bindings let it go */
      size_t len;
      const char *name = xb_memvar_name(s->id, &len);
      xb_set_str(&xb_array_items(a)[k], xb_str_new(name, len)); /* `b` does not change: nothing runs while it is read */
    }
    xb_array_items(a)[k + 1] = s->cell;
    k += 2;
    xb_gc_write(a);
  }
}

void xb_memvar_bindings_take(xb_env *e) {
  if (e->bindings) xb_atomic_add32(&e->bindings->users, 1, XB_RELAXED);
}

void xb_memvar_restore(xb_ctx *ctx, const xb_item *pairs) {
  if (pairs->type != XB_T_ARRAY) return;
  size_t n = xb_array_length(pairs->v.p);
  for (size_t k = 0; k + 1 < n; k += 2) {
    xb_item name;
    xb_array_get(pairs->v.p, k, &name);
    uint32_t id = name.type == XB_T_STRING ? xb_memvar_hold(xb_str_data(name.v.s), name.v.s->len) : (uint32_t)name.v.i;
    xb_array_get(pairs->v.p, k + 1, bindings_put(ctx->env, id));
    if (name.type == XB_T_STRING) xb_memvar_unhold(id);
  }
}

/* An id of `ids` (sorted) is `id`. */
static int has_id(const uint32_t *ids, uint32_t n, uint32_t id) {
  uint32_t lo = 0, hi = n;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (ids[mid] < id) lo = mid + 1;
    else hi = mid;
  }
  return lo < n && ids[lo] == id;
}

static int by_number(const void *a, const void *b) {
  uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
  return (x > y) - (x < y);
}

typedef struct id_list {
  uint32_t *ids;
  uint32_t n, cap;
} id_list;

static void id_push(id_list *l, uint32_t id) {
  if (l->n == l->cap) {
    l->cap = l->cap ? l->cap * 2 : 16;
    uint32_t *grown = realloc(l->ids, l->cap * sizeof *grown);
    if (!grown) xb_gc_fatal("out of memory");
    l->ids = grown;
  }
  l->ids[l->n++] = id;
}

uint32_t *xb_memvar_visible(xb_ctx *ctx, uint32_t *count) {
  const xb_bindings *b = ctx->env->bindings;
  id_list l = {NULL, 0, 0};
  xb_rwlock_read_lock(names_lock()); /* the PUBLICs' numbers stay theirs until held */
  for (uint32_t i = 0; b && i <= b->mask; ++i)
    if (b->slots[i].id != NO_ID && b->slots[i].cell.type == XB_T_CELL) id_push(&l, b->slots[i].id);
  if (l.n) qsort(l.ids, l.n, sizeof *l.ids, by_number);
  uint32_t bound = l.n;
  for (uint32_t id = 0, all = xb_atomic_load32(&numbered, XB_ACQUIRE); id < all; ++id) {
    xb_item cell;
    if (public_get(id, &cell) && !has_id(l.ids, bound, id)) id_push(&l, id);
  }
  uint32_t *ids = l.ids;
  uint32_t n = l.n;
  for (uint32_t i = 0; i < n; ++i) hold(ids[i]);
  xb_rwlock_read_unlock(names_lock());
  if (n) qsort(ids, n, sizeof *ids, by_number);
  *count = n;
  return ids;
}

/* __MemNamesInUse( [lLetGo] ): the names numbered and not freed (see the top of the file): the program's own
 * (permanent), and those something holds now, the threads' caches of names (xb_memvar_use) too; with .T., the calling
 * thread lets go of the names its cache holds first. */
void xbf___MEMNAMESINUSE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc && argv[0].type == XB_T_LOGICAL && argv[0].v.l) xb_memvar_cache_drop(ctx);
  xb_rwlock_read_lock(names_lock());
  uint32_t n = numbered - freeLen;
  xb_rwlock_read_unlock(names_lock());
  xb_set_int(ret, n);
}
