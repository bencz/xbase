/* Shared objects: the header bit, sharing a graph, and the stripes of sequence counters and locks (core/shared.h). */
#include "xbrt_int.h"

#include "core/shared.h"

#include <stdlib.h>
#include <string.h>

/* Each stripe on a cache line of its own: `seq` is odd while a writer works, `lock` is 0 when free, 1 when held and 2
 * when held with a thread waiting. Objects are XB_GC_ALIGN-aligned and at least two of those apart. */
#define STRIPES 1024u
#define SPINS 64u
typedef struct stripe {
  uint32_t seq;
  uint32_t lock;
  char pad[64 - 2 * sizeof(uint32_t)];
} stripe;
static _Alignas(64) stripe stripes[STRIPES];

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

typedef struct wait_args {
  uint32_t *lock;
} wait_args;

static void wait_lock(void *p) { xb_park_wait(((wait_args *)p)->lock, 2, -1); }

void xb_seq_lock(const void *obj) {
  stripe *s = stripe_of(obj);
  uint32_t none = 0;
  if (!xb_atomic_cas32(&s->lock, &none, 1, XB_ACQUIRE)) {
    for (unsigned spins = 0;; ++spins) {
      if (spins < SPINS) {
        none = 0;
        if (xb_atomic_cas32(&s->lock, &none, 1, XB_ACQUIRE)) break;
        continue;
      }
      /* Marked as waited for, then asleep until the holder wakes it; meanwhile collections go on without it. */
      if (xb_atomic_xchg32(&s->lock, 2, XB_ACQUIRE) == 0) break;
      wait_args a = {&s->lock};
      xb_waiting_run(wait_lock, &a); /* a writer's work is short */
    }
  }
  xb_atomic_add32(&s->seq, 1, XB_RELAXED); /* odd: readers wait or read again (before the writes: release stores) */
}

void xb_seq_unlock(const void *obj) {
  stripe *s = stripe_of(obj);
  xb_atomic_add32(&s->seq, 1, XB_RELEASE); /* even, after the writes */
  if (xb_atomic_xchg32(&s->lock, 0, XB_RELEASE) == 2) xb_park_wake(&s->lock, 0);
}

void xb_seq_put(const void *obj, xb_item *dst, const xb_item *src) {
  xb_seq_lock(obj);
  xb_seq_store(dst, src);
  xb_seq_unlock(obj);
}

/* The walk of xb_share: objects whose bit it set, still to visit. */
typedef struct walk {
  void **objs;
  size_t len, cap;
} walk;

static void visit(walk *w, const xb_item *it) {
  if (!xb_type_is_heap(it->type)) return;
  if (it->type == XB_T_STRING && (it->v.s->flags & XB_STR_STATIC)) return; /* no heap object: it never changes */
  void *obj = it->v.p;
  if (xb_atomic_or8(&xb_gc_header(obj)->state, XB_GC_SHARED, XB_RELAXED) & XB_GC_SHARED) return; /* shared already */
  if (w->len == w->cap) {
    size_t n = w->cap ? w->cap * 2 : 64;
    void **list = realloc(w->objs, n * sizeof *list);
    if (!list) xb_gc_fatal("out of memory");
    w->objs = list;
    w->cap = n;
  }
  w->objs[w->len++] = obj;
}

void xb_share(const xb_item *it) {
  walk w = {NULL, 0, 0};
  visit(&w, it);
  while (w.len) {
    void *obj = w.objs[--w.len];
    switch ((xb_gc_kind)xb_gc_header(obj)->kind) {
      case XB_GC_STRING: xb_str_share(obj); break;
      case XB_GC_ARRAY: {
        xb_array *a = obj;
        for (size_t i = 0; i < a->len; ++i) visit(&w, &a->items[i]);
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
  free(w.objs);
}
