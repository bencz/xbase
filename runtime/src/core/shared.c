/* Shared objects: the header bit, sharing a graph, and the stripes of sequence counters and locks (core/shared.h). */
#include "xbrt_int.h"

#include "core/shared.h"

#include <stdlib.h>
#include <string.h>

/* Each stripe on a cache line of its own: `seq` is odd while a writer works, `lock` is the writers' lock (its states
 * below, at lock_slow). Objects are XB_GC_ALIGN-aligned and at least two of those apart. */
#define STRIPES 1024u
#define SPINS 64u
typedef struct stripe {
  uint32_t seq;
  uint32_t lock;
  char pad[XB_CACHE_LINE - 2 * sizeof(uint32_t)];
} stripe;
static _Alignas(XB_CACHE_LINE) stripe stripes[STRIPES];

static stripe *stripe_of(const void *obj) {
  uintptr_t a = (uintptr_t)obj / 16u;
  return &stripes[(a ^ (a >> 10)) & (STRIPES - 1u)];
}

/* An item's words, read and written atomically one by one, as wide as items are aligned (8 bytes, or 4 where items
 * are 4-aligned, as on i386): a reader that a writer overlaps sees each word whole, and the sequence counter tells it
 * to read again. A word is stored with release order, so the counter made odd before it is seen first, and loaded
 * with acquire order, so the counter read after it is read after it (no fences: the thread sanitizer follows these). */
_Static_assert(sizeof(xb_item) == 16, "an item is two 8-byte words (xbrt.h)");
static const int wide = _Alignof(xb_item) >= 8; /* a variable, not a constant: no warning for the test of it */
#if defined(__GNUC__) || defined(__clang__)
typedef uint64_t __attribute__((may_alias)) word64;
typedef uint32_t __attribute__((may_alias)) word32;
static uint64_t load64(const char *p) { return __atomic_load_n((const word64 *)(const void *)p, __ATOMIC_ACQUIRE); }
static uint32_t load32(const char *p) { return __atomic_load_n((const word32 *)(const void *)p, __ATOMIC_ACQUIRE); }
static void store64(char *p, uint64_t v) { __atomic_store_n((word64 *)(void *)p, v, __ATOMIC_RELEASE); }
static void store32(char *p, uint32_t v) { __atomic_store_n((word32 *)(void *)p, v, __ATOMIC_RELEASE); }
#else
static uint64_t load64(const char *p) { return xb_atomic_load64((const uint64_t *)(const void *)p, XB_ACQUIRE); }
static uint32_t load32(const char *p) { return xb_atomic_load32((const uint32_t *)(const void *)p, XB_ACQUIRE); }
static void store64(char *p, uint64_t v) { xb_atomic_store64((uint64_t *)(void *)p, v, XB_RELEASE); }
static void store32(char *p, uint32_t v) { xb_atomic_store32((uint32_t *)(void *)p, v, XB_RELEASE); }
#endif

void xb_seq_load(xb_item *dst, const xb_item *src) {
  const char *p = (const char *)src;
  if (wide) {
    uint64_t w[2] = {load64(p), load64(p + 8)};
    memcpy(dst, w, sizeof w);
  } else {
    uint32_t w[4] = {load32(p), load32(p + 4), load32(p + 8), load32(p + 12)};
    memcpy(dst, w, sizeof w);
  }
}

void xb_seq_store(xb_item *dst, const xb_item *src) {
  char *p = (char *)dst;
  if (wide) {
    uint64_t w[2];
    memcpy(w, src, sizeof w);
    store64(p, w[0]);
    store64(p + 8, w[1]);
  } else {
    uint32_t w[4];
    memcpy(w, src, sizeof w);
    for (int k = 0; k < 4; ++k) store32(p + 4 * k, w[k]);
  }
}

static void back_off(unsigned spins) {
  if (spins >= SPINS) xb_thread_yield();
}

uint32_t xb_seq_read_begin(const void *obj) {
  const uint32_t *seq = &stripe_of(obj)->seq;
  for (unsigned spins = 0;; ++spins) {
    uint32_t s = xb_atomic_load32(seq, XB_ACQUIRE);
    if (!(s & 1u)) return s;
    back_off(spins); /* a writer, which reaches no safepoint before it ends: no collection waits for this */
  }
}

int xb_seq_read_valid(const void *obj, uint32_t seq) {
  return xb_atomic_load32(&stripe_of(obj)->seq, XB_RELAXED) == seq; /* after the items' acquire loads */
}

void xb_seq_get(const void *obj, xb_item *dst, const xb_item *src) {
  for (;;) {
    uint32_t seq = xb_seq_read_begin(obj);
    xb_seq_load(dst, src);
    if (xb_seq_read_valid(obj, seq)) return;
  }
}

/* A stripe's `lock`: free (0), held (LOCK_HELD), held while threads may sleep waiting for it (LOCK_WAITED: its release
 * wakes the first), and so with a waiter that has waited STARVING_NS (LOCK_STARVING: its release hands the lock over,
 * LOCK_HANDED, instead of freeing it). A handed lock is free for a thread that has slept waiting, not for a newcomer.
 * Without the hand-over, a thread that takes the lock again and again would find it free at each release and take it
 * before the woken thread could, which would find it taken again. */
#define LOCK_HELD 1u
#define LOCK_WAITED 2u
#define LOCK_STARVING 4u
#define LOCK_HANDED 8u
#define STARVING_NS 1000000

static void lock_slow(stripe *s) {
  int slept = 0, starving = 0;
  int64_t since = xb_platform_monotonic_ns();
  for (;;) {
    uint32_t v = xb_atomic_load32(&s->lock, XB_RELAXED);
    if (v == 0 || (v == LOCK_HANDED && slept)) { /* LOCK_WAITED: other threads may still sleep waiting */
      if (xb_atomic_cas32(&s->lock, &v, LOCK_HELD | LOCK_WAITED, XB_ACQUIRE)) return;
      continue;
    }
    uint32_t seen = v == LOCK_HANDED ? v : v | LOCK_WAITED | (starving ? LOCK_STARVING : 0u);
    if (seen != v && !xb_atomic_cas32(&s->lock, &v, seen, XB_RELAXED)) continue;
    xb_park_wait_short(&s->lock, seen, -1); /* a writer's work is short; collections go on without this thread */
    slept = 1;
    starving = starving || xb_platform_monotonic_ns() - since >= STARVING_NS;
  }
}

void xb_seq_lock(const void *obj) {
  stripe *s = stripe_of(obj);
  uint32_t none = 0;
  if (!xb_atomic_cas32(&s->lock, &none, LOCK_HELD, XB_ACQUIRE)) {
    unsigned spins = 0;
    for (; spins < SPINS; ++spins) { /* a load first: a compare-and-swap takes the line from the holder each time */
      none = 0;
      if (!xb_atomic_load32(&s->lock, XB_RELAXED) && xb_atomic_cas32(&s->lock, &none, LOCK_HELD, XB_ACQUIRE)) break;
    }
    if (spins == SPINS) lock_slow(s);
  }
  xb_atomic_add32(&s->seq, 1, XB_RELAXED); /* odd: readers wait or read again */
  /* The odd count is seen before any write that follows, plain stores too (a hash's words, xb_seq_read_valid's
   * readers on weak memory: ARM, POWER): a reader that sees a write sees the count changed. */
  xb_atomic_fence(XB_RELEASE);
}

void xb_seq_unlock(const void *obj) {
  stripe *s = stripe_of(obj);
  xb_atomic_add32(&s->seq, 1, XB_RELEASE); /* even, after the writes */
  uint32_t v = xb_atomic_load32(&s->lock, XB_RELAXED);
  while (!xb_atomic_cas32(&s->lock, &v, v & LOCK_STARVING ? LOCK_HANDED : 0u, XB_RELEASE)) {
  }
  if (v & LOCK_WAITED) xb_park_wake(&s->lock, 0);
}

void xb_seq_put(const void *obj, xb_item *dst, const xb_item *src) {
  xb_seq_lock(obj);
  xb_seq_store(dst, src);
  xb_seq_unlock(obj);
}

int xb_seq_int_step(xb_item *it, xb_int_step next, const int64_t *args, int64_t *was) {
  if (!wide) return 0;
  uint64_t *word = (uint64_t *)(void *)&it->v.i;
  uint64_t old = xb_atomic_load64(word, XB_RELAXED);
  for (;;) {
    int64_t changed;
    if (!next((int64_t)old, args, &changed) || xb_atomic_cas64(word, &old, (uint64_t)changed, XB_SEQ_CST)) break;
  }
  *was = (int64_t)old;
  return 1;
}

/* The walk of xb_share: objects whose bit it set, still to visit; in `local`, on the stack, until they outgrow it. */
#define WALK_LOCAL 64u
typedef struct walk {
  void **objs;
  size_t len, cap;
  void *local[WALK_LOCAL];
} walk;

static void visit(walk *w, const xb_item *it) {
  if (!xb_type_is_heap(it->type)) return;
  if (it->type == XB_T_STRING && (it->v.s->flags & XB_STR_STATIC)) return; /* no heap object: it never changes */
  void *obj = it->v.p;
  /* shared already: a load sees it (sharing is never undone), else the read-modify-write decides between threads */
  if (xb_atomic_load8(&xb_gc_header(obj)->state, XB_RELAXED) & XB_GC_SHARED) return;
  if (xb_atomic_or8(&xb_gc_header(obj)->state, XB_GC_SHARED, XB_RELAXED) & XB_GC_SHARED) return;
  if (w->len == w->cap) {
    size_t n = w->cap * 2;
    void **list = w->objs == w->local ? malloc(n * sizeof *list) : realloc(w->objs, n * sizeof *list);
    if (!list) xb_gc_fatal("out of memory");
    if (w->objs == w->local) memcpy(list, w->local, sizeof w->local);
    w->objs = list;
    w->cap = n;
  }
  w->objs[w->len++] = obj;
}

void xb_share_walk(const xb_item *it) {
  walk w;
  w.objs = w.local;
  w.len = 0;
  w.cap = WALK_LOCAL;
  visit(&w, it);
  while (w.len) {
    void *obj = w.objs[--w.len];
    switch ((xb_gc_kind)xb_gc_header(obj)->kind) {
      case XB_GC_STRING: xb_str_share(obj); break;
      case XB_GC_ARRAY: {
        xb_array *a = obj;
        const xb_item *items = xb_array_items(a); /* a shared array holds items: it turns before others reach it */
        for (size_t i = 0; i < a->len; ++i) visit(&w, &items[i]);
        break;
      }
      case XB_GC_BLOCK: {
        xb_block *b = obj;
        for (uint32_t i = 0; i < b->ncells; ++i)
          if (b->cells[i]) {
            xb_item cell;
            xb_set_ptr(&cell, XB_T_CELL, b->cells[i]);
            visit(&w, &cell);
          }
        break;
      }
      case XB_GC_CELL: visit(&w, &((xb_cell *)obj)->value); break;
      case XB_GC_REF: {
        xb_ref *r = obj;
        visit(&w, &r->coll);
        visit(&w, &r->value);
        visit(&w, &r->key);
        break;
      }
      case XB_GC_OBJECT: {
        xb_object *o = obj;
        for (uint32_t i = 0; i < o->nvars; ++i) visit(&w, &o->vars[i]);
        break;
      }
      case XB_GC_HASH: {
        xb_hash *h = obj;
        for (uint32_t i = 0; i < h->used; ++i) {
          visit(&w, &h->entries[i].key);
          visit(&w, &h->entries[i].value);
        }
        break;
      }
      default: break; /* items and stores are reached through their owners, under their owners' stripes */
    }
  }
  if (w.objs != w.local) free(w.objs);
}
