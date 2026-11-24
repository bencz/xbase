/* Shared objects (core/shared.h). Sharing marks a whole graph once (a cycle, nested arrays, a hash, a codeblock and its
 * cell, strings: the empty and one-byte ones, which are static, are left alone, a growing one stops growing in place),
 * and stops at what is shared already. Then writers put items of two kinds into one shared array under its lock while
 * readers take them whole, collections (full ones, which move objects, too) running meanwhile, and now and then a
 * writer keeps the lock for a while, so the other one sleeps for it: an integer item is always below 2^32 and a double
 * item at least 0.5, so a reader that saw the type of one value with the other's word would see an integer of a
 * double's bits or a tiny double. None does. */
#include "check.h"
#include "core/shared.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <string.h>

#define WRITERS 2
#define READERS 3
#define ITEMS 8u
#define ROUNDS 200000u

static xb_item shared;         /* the array the threads share, a root */
static uint32_t bad, writersLeft = WRITERS;

static int is_shared(const xb_item *it) { return xb_is_shared(it->v.p); }

static void graph(void) {
  xb_ctx *ctx = xb_ctx_new();
  xb_item a, inner, h, key, val, blk, grown, tail;
  xb_item *roots[] = {&a, &inner, &h, &key, &val, &blk, &grown, &tail};
  for (size_t k = 0; k < sizeof roots / sizeof *roots; ++k) {
    xb_set_nil(roots[k]);
    xb_gc_root_push(ctx, roots[k]);
  }
  xb_set_array(&a, xb_array_alloc(7));
  xb_set_array(&inner, xb_array_alloc(1));
  xb_set_str(&xb_array_items(inner.v.p)[0], xb_str_new("deep", 4));
  xb_set_hash(&h, xb_hash_alloc());
  xb_set_str(&key, xb_str_new("key", 3)); /* longer than a byte: a string of one byte is static */
  xb_set_str(&val, xb_str_new("value", 5));
  xb_hash_set(h.v.p, &key, &val);
  xb_block_new(ctx, &blk, NULL, NULL, 0, 1);
  xb_item var;
  xb_set_int(&var, 7);
  xb_cell *cell = xb_cell_move(ctx, &var, 0); /* `var` now holds the cell: the block's from here on */
  ((xb_block *)blk.v.p)->cells[0] = cell;
  xb_set_str(&grown, xb_str_new("ab", 2));
  xb_set_str(&tail, xb_str_new("cd", 2));
  xb_op_append(ctx, &grown, &tail); /* a string with room to grow in place */
  CHECK(grown.v.s->flags & XB_STR_APPEND);
  xb_array *arr = a.v.p;
  xb_item *items = xb_array_items(arr);
  items[0] = a; /* a cycle */
  items[1] = inner;
  items[2] = h;
  items[3] = blk;
  items[4] = grown;
  xb_set_str(&items[5], xb_str_new("x", 1)); /* static: one byte, and empty */
  xb_set_str(&items[6], xb_str_new("", 0));
  xb_share(&a);
  CHECK(is_shared(&a) && is_shared(&inner) && is_shared(&xb_array_items(inner.v.p)[0]));
  CHECK(is_shared(&h) && xb_is_shared(cell) && is_shared(&blk) && is_shared(&grown));
  CHECK(!(grown.v.s->flags & XB_STR_APPEND));
  xb_hentry *e = ((xb_hash *)h.v.p)->entries;
  CHECK(xb_is_shared(e[0].key.v.s) && xb_is_shared(e[0].value.v.s));
  xb_gc_collect(); /* the bit outlives collections, full and young */
  xb_gc_collect_young();
  CHECK(is_shared(&a) && is_shared(&inner) && is_shared(&grown));
  xb_item fresh;
  xb_set_array(&fresh, xb_array_alloc(1));
  xb_array_items(fresh.v.p)[0] = a; /* shared already: the walk stops there */
  xb_share(&fresh);
  CHECK(is_shared(&fresh));
  xb_gc_root_pop(ctx, (unsigned)(sizeof roots / sizeof *roots));
  xb_ctx_free(ctx);
}

static void writer(void *arg) {
  uint32_t me = (uint32_t)(size_t)arg;
  xb_gc_thread_attach(&arg);
  xb_ctx *ctx = xb_ctx_new();
  xb_array *a = shared.v.p; /* read again from its root after each safepoint: a full collection may move it */
  for (uint32_t round = 0; round < ROUNDS; ++round) {
    xb_item v;
    if ((round / ITEMS + me) & 1u) xb_set_int(&v, (int64_t)round); /* each item gets both kinds, by turns */
    else xb_set_double(&v, 0.5 + (double)round, 1);
    size_t at = (round * 7u + me) % ITEMS;
    xb_seq_put(a, &xb_array_items(a)[at], &v);
    if (me == 0 && round % 50000u == 0) { /* the lock kept a while: the other writer sleeps for it */
      xb_seq_lock(a);
      xb_platform_sleep_ms(3);
      xb_seq_unlock(a);
    }
    if (round % 4096u == 0) {
      if (xb_pending_get(ctx)) xb_pending_gc(ctx);
      if (me == 1 && round % 40960u == 0) xb_gc_collect(); /* full: objects may move meanwhile */
      a = shared.v.p;
    }
  }
  xb_atomic_add32(&writersLeft, (uint32_t)-1, XB_RELEASE);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

static void reader(void *arg) {
  xb_gc_thread_attach(&arg);
  xb_ctx *ctx = xb_ctx_new();
  xb_array *a = shared.v.p;
  uint32_t n = 0;
  while (xb_atomic_load32(&writersLeft, XB_ACQUIRE)) {
    xb_item v;
    xb_seq_get(a, &v, &xb_array_items(a)[n++ % ITEMS]);
    if (v.type == XB_T_INT ? (uint64_t)v.v.i >= ((uint64_t)1 << 32)
                           : v.type == XB_T_DOUBLE ? !(v.v.d >= 0.5) : v.type != XB_T_NIL)
      xb_atomic_add32(&bad, 1, XB_RELAXED);
    if (n % 65536u == 0) {
      if (xb_pending_get(ctx)) xb_pending_gc(ctx);
      if (n % 262144u == 0) xb_gc_collect_young(); /* collections while the others read and write */
      a = shared.v.p;
    }
  }
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  graph();
  xb_set_array(&shared, xb_array_alloc(ITEMS));
  xb_gc_add_root(&shared, 1);
  xb_share(&shared);
  xb_gc_thread_detach();
  xb_thread threads[WRITERS + READERS];
  for (size_t k = 0; k < WRITERS; ++k) CHECK(xb_thread_start(&threads[k], (size_t)8 << 20, writer, (void *)k));
  for (size_t k = 0; k < READERS; ++k) CHECK(xb_thread_start(&threads[WRITERS + k], (size_t)8 << 20, reader, NULL));
  for (size_t k = 0; k < WRITERS + READERS; ++k) xb_thread_join(threads[k]);
  CHECK(bad == 0);
  puts("shared ok");
  return 0;
}
