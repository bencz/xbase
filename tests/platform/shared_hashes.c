/* The runtime's hash operations on a shared hash (types/hash.c, rtl/hashes.c), threads at once, collections (full
 * ones, which move objects, too) running meanwhile. First putters HSet() keys of their own (strings, shared as they
 * are stored; the hash's store grows many times) while readers look keys up and take HKeys(): the keys the hash held
 * before are found at every lookup (a lookup under the sequence counter that a growth overlaps reads again, never
 * misses), and in the end every key is there with its value. Then writers set values (alternating integers and
 * doubles), HDel(), remove by position, HSort(), HFill(), HCopy() into it, HAllocate(), HCaseMatch(), HValueAt() and
 * test pairs ($) while readers look keys up and take entries by position: a reader never sees an integer of a double's
 * bits or a tiny double (an item read torn), nor a value of another type, and in the end each key is there once, at
 * its position. Then one thread sorts the hash over and over while another sets its values: no sort brings an older
 * value back. Last, a small hash (a store without an index, searched in order) of four keys that never change, while
 * a writer adds and deletes four others (holes, compactions), turns its letter case rule off and on (every hash
 * computed again), then adds keys past the small store's size (the store becomes one with an index): readers find
 * the four keys at every lookup, and the others with their values or not at all. */
#include "check.h"
#include "core/shared.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void xbf_HSET(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_HDEL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_HKEYS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_HSORT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_HFILL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_HCOPY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_HALLOCATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbf_HCASEMATCH(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

#define PUTTERS 3
#define READERS 2
#define PUTS 8000u
#define MIXERS 2
#define MIXES 20000u
#define KEYS 64u
#define BASE_KEYS 32u /* in the hash before the putters start: every lookup finds them */
#define FIXED_KEYS 4u  /* in the small hash throughout */
#define CHURNS 40000u
#define GROWN_KEYS 24u

static xb_item shared;
static uint32_t bad, busy;

typedef struct worker {
  xb_ctx *ctx;
  xb_item args[3], ret;
} worker;

static void begin(worker *w, void *base) {
  xb_gc_thread_attach(base);
  w->ctx = xb_ctx_new();
  for (int k = 0; k < 3; ++k) {
    xb_set_nil(&w->args[k]);
    xb_gc_root_push(w->ctx, &w->args[k]);
  }
  xb_set_nil(&w->ret);
  xb_gc_root_push(w->ctx, &w->ret);
}

static void end(worker *w) {
  xb_gc_root_pop(w->ctx, 4);
  xb_ctx_free(w->ctx);
  xb_gc_thread_detach();
}

static void poll(worker *w, uint32_t n) {
  if (xb_pending_get(w->ctx)) xb_pending_gc(w->ctx);
  if (n % 4096u == 2048u) xb_gc_collect(); /* full: objects may move */
  else if (n % 1024u == 0) xb_gc_collect_young();
}

static void key(xb_item *it, const char *prefix, uint32_t n) {
  char buf[32];
  int len = snprintf(buf, sizeof buf, "%s%u", prefix, n);
  xb_set_str(it, xb_str_new(buf, (size_t)len));
}

static void putter(void *arg) {
  worker w;
  begin(&w, &arg);
  uint32_t me = (uint32_t)(size_t)arg;
  for (uint32_t j = 0; j < PUTS; ++j) {
    w.args[0] = shared;
    key(&w.args[1], "k", me * PUTS + j);
    xb_set_int(&w.args[2], (int64_t)(me * PUTS + j));
    xbf_HSET(w.ctx, &w.ret, 3, w.args);
    poll(&w, j);
  }
  xb_atomic_add32(&busy, (uint32_t)-1, XB_RELEASE);
  end(&w);
}

/* A value read from the hash in the second phase: NIL, an integer below 2^32 or a double of at least 0.5. */
static int valid(const xb_item *v) {
  if (v->type == XB_T_NIL) return 1;
  if (v->type == XB_T_INT) return (uint64_t)v->v.i < ((uint64_t)1 << 32);
  return v->type == XB_T_DOUBLE && v->v.d >= 0.5;
}

static void reader(void *arg) {
  worker w;
  begin(&w, &arg);
  int mixed = (int)(size_t)arg;
  for (uint32_t n = 0; xb_atomic_load32(&busy, XB_ACQUIRE); ++n) {
    w.args[0] = shared;
    if (mixed == 2) {
      key(&w.args[1], "f", n % FIXED_KEYS);
      if (!xb_hash_get(shared.v.p, &w.args[1], &w.ret) || w.ret.type != XB_T_INT ||
          w.ret.v.i != (int64_t)(n % FIXED_KEYS))
        xb_atomic_add32(&bad, 1, XB_RELAXED);
      key(&w.args[1], "x", n % FIXED_KEYS);
      if (xb_hash_get(shared.v.p, &w.args[1], &w.ret) && w.ret.type != XB_T_INT) xb_atomic_add32(&bad, 1, XB_RELAXED);
    } else if (!mixed) {
      uint32_t k = n % (PUTTERS * PUTS);
      key(&w.args[1], "k", k);
      if (xb_hash_get(shared.v.p, &w.args[1], &w.ret) && !(w.ret.type == XB_T_INT && w.ret.v.i == (int64_t)k))
        xb_atomic_add32(&bad, 1, XB_RELAXED);
      key(&w.args[1], "b", n % BASE_KEYS);
      if (!xb_hash_get(shared.v.p, &w.args[1], &w.ret) || w.ret.type != XB_T_INT ||
          w.ret.v.i != (int64_t)(n % BASE_KEYS))
        xb_atomic_add32(&bad, 1, XB_RELAXED);
      if (n % 509u == 0) {
        xbf_HKEYS(w.ctx, &w.ret, 1, w.args);
        const xb_array *keys = w.ret.v.p;
        for (size_t i = 0; i < keys->len; ++i)
          if (xb_array_items(keys)[i].type != XB_T_STRING) xb_atomic_add32(&bad, 1, XB_RELAXED);
      }
    } else {
      key(&w.args[1], "s", n % KEYS);
      if (xb_hash_get(shared.v.p, &w.args[1], &w.ret) && !valid(&w.ret)) xb_atomic_add32(&bad, 1, XB_RELAXED);
      if (xb_hash_entry(shared.v.p, (int64_t)(n % KEYS) + 1, &w.args[1], &w.ret) &&
          (w.args[1].type != XB_T_STRING || !valid(&w.ret)))
        xb_atomic_add32(&bad, 1, XB_RELAXED);
    }
    poll(&w, n);
  }
  end(&w);
}

/* The writer of the small hash: HSet and HDel of keys "x0" to "x3", HCaseMatch off and on, then keys past the small
 * store's size. */
static void churner(void *arg) {
  worker w;
  begin(&w, &arg);
  for (uint32_t j = 0; j < CHURNS + GROWN_KEYS; ++j) {
    w.args[0] = shared;
    if (j >= CHURNS) {
      key(&w.args[1], "g", j - CHURNS);
      xb_set_int(&w.args[2], (int64_t)j);
      xbf_HSET(w.ctx, &w.ret, 3, w.args);
    } else if (j % 512u == 0) {
      xb_set_logical(&w.args[1], (j / 512u) % 2 == 0);
      xbf_HCASEMATCH(w.ctx, &w.ret, 2, w.args);
    } else {
      key(&w.args[1], "x", j % FIXED_KEYS);
      xb_set_int(&w.args[2], (int64_t)j);
      xbf_HSET(w.ctx, &w.ret, 3, w.args);
      key(&w.args[1], "x", (j + 2) % FIXED_KEYS);
      xbf_HDEL(w.ctx, &w.ret, 2, w.args);
    }
    poll(&w, j);
  }
  xb_atomic_add32(&busy, (uint32_t)-1, XB_RELEASE);
  end(&w);
}

static void mixer(void *arg) {
  worker w;
  begin(&w, &arg);
  uint32_t me = (uint32_t)(size_t)arg;
  for (uint32_t j = 0; j < MIXES; ++j) {
    w.args[0] = shared;
    key(&w.args[1], "s", (j * 13u + me) % KEYS);
    switch ((j + me) % 20u) {
      case 0: xbf_HDEL(w.ctx, &w.ret, 2, w.args); break;
      case 1: xb_hash_remove_at(shared.v.p, (int64_t)(j % KEYS) + 1); break; /* 0 past the end: no error */
      case 2: xbf_HSORT(w.ctx, &w.ret, 1, w.args); break;
      case 3:
        xb_set_int(&w.args[1], (int64_t)j);
        xbf_HFILL(w.ctx, &w.ret, 2, w.args);
        break;
      case 4: { /* a pair: { key => its value } $ hash */
        xb_item pair;
        xb_set_hash(&w.ret, xb_hash_alloc());
        xb_set_int(&pair, 1);
        xb_hash_set(w.ret.v.p, &w.args[1], &pair);
        xb_op_in(w.ctx, &w.args[2], &w.ret, &w.args[0]);
        if (w.args[2].type != XB_T_LOGICAL) xb_atomic_add32(&bad, 1, XB_RELAXED);
        break;
      }
      case 5: { /* HCopy( other, shared ): keys of another store put into this one */
        xb_item v;
        w.args[0] = w.args[1];
        xb_set_hash(&w.args[2], xb_hash_alloc());
        xb_set_int(&v, (int64_t)j);
        xb_hash_set(w.args[2].v.p, &w.args[0], &v);
        w.args[0] = w.args[2];
        w.args[1] = shared;
        xbf_HCOPY(w.ctx, &w.ret, 2, w.args);
        break;
      }
      case 6:
        xb_set_int(&w.args[1], (int64_t)(KEYS + j % 64u));
        xbf_HALLOCATE(w.ctx, &w.ret, 2, w.args);
        break;
      case 7: /* the keys differ in more than letter case: no two become one */
        xb_set_logical(&w.args[1], (j / 20u) & 1u);
        xbf_HCASEMATCH(w.ctx, &w.ret, 2, w.args);
        break;
      case 8:
        xb_set_int(&w.args[2], (int64_t)j);
        xb_hash_value_at(shared.v.p, (int64_t)(j % KEYS) + 1, &w.args[2], &w.ret); /* 0 past the end */
        break;
      default:
        if ((j / 16u + me) & 1u) xb_set_int(&w.args[2], (int64_t)j);
        else xb_set_double(&w.args[2], 0.5 + (double)j, 1);
        xbf_HSET(w.ctx, &w.ret, 3, w.args);
    }
    poll(&w, j);
  }
  xb_atomic_add32(&busy, (uint32_t)-1, XB_RELEASE);
  end(&w);
}

/* Each key once, at the position it is found at. */
static void integrity(void) {
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_item k;
  xb_set_nil(&k);
  xb_gc_root_push(ctx, &k);
  uint32_t len = xb_hash_length(shared.v.p);
  for (uint32_t pos = 1; pos <= len; ++pos) {
    CHECK(xb_hash_entry(shared.v.p, pos, &k, NULL) && k.type == XB_T_STRING);
    CHECK(xb_hash_position(shared.v.p, &k) == (int64_t)pos);
  }
  xb_gc_root_pop(ctx, 1);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

static uint32_t sorting;
static int64_t lastSet[KEYS];

static void sorter(void *arg) {
  worker w;
  begin(&w, &arg);
  for (uint32_t n = 0; xb_atomic_load32(&sorting, XB_ACQUIRE); ++n) {
    w.args[0] = shared;
    xbf_HSORT(w.ctx, &w.ret, 1, w.args);
    poll(&w, n);
  }
  end(&w);
}

static void setter(void *arg) {
  worker w;
  begin(&w, &arg);
  for (uint32_t j = 1; j <= MIXES; ++j) {
    uint32_t k = j % KEYS;
    w.args[0] = shared;
    key(&w.args[1], "s", k);
    xb_set_int(&w.args[2], (int64_t)j);
    xbf_HSET(w.ctx, &w.ret, 3, w.args);
    lastSet[k] = j;
    poll(&w, j);
  }
  xb_atomic_store32(&sorting, 0, XB_RELEASE);
  end(&w);
}

/* One thread sorts, another sets: every key ends with the value set last. */
static void last_values(void) {
  xb_atomic_store32(&sorting, 1, XB_RELEASE);
  xb_thread t[2];
  CHECK(xb_thread_start(&t[0], (size_t)8 << 20, sorter, NULL));
  CHECK(xb_thread_start(&t[1], (size_t)8 << 20, setter, NULL));
  xb_thread_join(t[0]);
  xb_thread_join(t[1]);
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_item k, v;
  xb_set_nil(&k);
  xb_set_nil(&v);
  xb_gc_root_push(ctx, &k);
  xb_gc_root_push(ctx, &v);
  for (uint32_t n = 0; n < KEYS; ++n) {
    key(&k, "s", n);
    CHECK(xb_hash_get(shared.v.p, &k, &v) && v.type == XB_T_INT && v.v.i == lastSet[n]);
  }
  xb_gc_root_pop(ctx, 2);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

static void run(void (*fn)(void *), size_t writers, int mixed) {
  xb_thread threads[PUTTERS + READERS];
  xb_atomic_store32(&busy, (uint32_t)writers, XB_RELEASE);
  for (size_t k = 0; k < writers; ++k) CHECK(xb_thread_start(&threads[k], (size_t)8 << 20, fn, (void *)k));
  for (size_t k = 0; k < READERS; ++k)
    CHECK(xb_thread_start(&threads[writers + k], (size_t)8 << 20, reader, (void *)(size_t)mixed));
  for (size_t k = 0; k < writers + READERS; ++k) xb_thread_join(threads[k]);
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_set_hash(&shared, xb_hash_alloc());
  xb_gc_add_root(&shared, 1);
  xb_ctx *ctx = xb_ctx_new();
  xb_item k, v;
  xb_set_nil(&k);
  xb_set_nil(&v);
  xb_gc_root_push(ctx, &k);
  xb_gc_root_push(ctx, &v);
  for (uint32_t n = 0; n < BASE_KEYS; ++n) {
    key(&k, "b", n);
    xb_set_int(&v, (int64_t)n);
    xb_hash_set(shared.v.p, &k, &v);
  }
  xb_share(&shared);
  xb_gc_root_pop(ctx, 2);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  run(putter, PUTTERS, 0);
  CHECK(bad == 0);
  xb_gc_thread_attach(&base);
  ctx = xb_ctx_new();
  xb_set_nil(&k); /* what they held may have moved meanwhile */
  xb_set_nil(&v);
  xb_gc_root_push(ctx, &k);
  xb_gc_root_push(ctx, &v);
  CHECK(xb_hash_length(shared.v.p) == BASE_KEYS + PUTTERS * PUTS);
  for (uint32_t n = 0; n < PUTTERS * PUTS; ++n) {
    key(&k, "k", n);
    CHECK(xb_hash_get(shared.v.p, &k, &v) && v.type == XB_T_INT && v.v.i == (int64_t)n);
  }
  xb_set_hash(&shared, xb_hash_alloc());
  for (uint32_t n = 0; n < KEYS; ++n) {
    key(&k, "s", n);
    xb_set_int(&v, (int64_t)n);
    xb_hash_set(shared.v.p, &k, &v);
  }
  xb_share(&shared);
  xb_gc_root_pop(ctx, 2);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  run(mixer, MIXERS, 1);
  CHECK(bad == 0);
  integrity();
  last_values();
  xb_gc_thread_attach(&base);
  ctx = xb_ctx_new();
  xb_set_nil(&k);
  xb_set_nil(&v);
  xb_gc_root_push(ctx, &k);
  xb_gc_root_push(ctx, &v);
  xb_set_hash(&shared, xb_hash_alloc());
  for (uint32_t n = 0; n < FIXED_KEYS; ++n) {
    key(&k, "f", n);
    xb_set_int(&v, (int64_t)n);
    xb_hash_set(shared.v.p, &k, &v);
  }
  xb_share(&shared);
  xb_gc_root_pop(ctx, 2);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  run(churner, 1, 2);
  CHECK(bad == 0);
  CHECK(xb_hash_length(shared.v.p) == FIXED_KEYS + 2 + GROWN_KEYS);
  puts("shared_hashes ok");
  return 0;
}
