/* Appends (xb_op_add_grow, xb_str_add in types/string.c) to views shared by threads: each base string of a shared
 * array is a view that ends at its buffer's tip, and every thread appends a tag of its own to every base, in the same
 * order, so that they race for each tip, then appends to its result a few times more; collections (full ones, which
 * move objects, too) run meanwhile. Exactly one thread's result per base is written at that base's tip (its buffer),
 * the others are copies; every result holds the base's bytes and then its own thread's appends alone, and the bases
 * stay as they were. */
#include "check.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <string.h>

#define THREADS 4
#define BASES 20000u
#define MORE 3u
#define STEP 16u
#define BASE_FILL XB_STR_VIEW_MIN /* a base is a view */

static xb_item bases, results[THREADS];
static uint32_t bad, ready;

static size_t base_text(char *out, uint32_t k) {
  memset(out, 'b', BASE_FILL);
  return BASE_FILL + (size_t)snprintf(out + BASE_FILL, 32, "k%u|", k);
}

/* What thread `me` appends to base `k`: its tag, then MORE times "+<me>". */
static size_t appended_text(char *out, uint32_t me, uint32_t k) {
  size_t n = (size_t)snprintf(out, 32, "%u:%u;", me, k);
  for (uint32_t m = 0; m < MORE; ++m) n += (size_t)snprintf(out + n, 16, "+%u", me);
  return n;
}

static int holds(const xb_item *it, const char *bytes, size_t len) {
  return it->type == XB_T_STRING && it->v.s->len == len && memcmp(xb_str_data(it->v.s), bytes, len) == 0;
}

static void poll(xb_ctx *ctx, uint32_t n) {
  if (xb_pending_get(ctx)) xb_pending_gc(ctx);
  if (n % 4096u == 2048u) xb_gc_collect(); /* full: objects may move */
  else if (n % 1024u == 0) xb_gc_collect_young();
}

static void appender(void *arg) {
  uint32_t me = (uint32_t)(size_t)arg;
  xb_gc_thread_attach(&arg);
  xb_ctx *ctx = xb_ctx_new();
  xb_item base, piece, sum;
  xb_set_nil(&base);
  xb_set_nil(&piece);
  xb_set_nil(&sum);
  xb_gc_root_push(ctx, &base);
  xb_gc_root_push(ctx, &piece);
  xb_gc_root_push(ctx, &sum);
  char text[BASE_FILL + 128], tail[64];
  for (uint32_t k = 0; k < BASES; ++k) {
    if (k % STEP == 0) { /* every thread at the same bases, to race for their tips */
      xb_atomic_add32(&ready, 1, XB_RELEASE);
      while (xb_atomic_load32(&ready, XB_ACQUIRE) < THREADS * (k / STEP + 1)) {
        if (xb_pending_get(ctx)) xb_pending_gc(ctx);
        xb_thread_yield();
      }
    }
    CHECK(xb_array_get(bases.v.p, k, &base));
    int n = snprintf(tail, sizeof tail, "%u:%u;", me, k);
    xb_set_str(&piece, xb_str_new(tail, (size_t)n));
    xb_op_add_grow(ctx, &sum, &base, &piece);
    for (uint32_t m = 0; m < MORE; ++m) {
      n = snprintf(tail, sizeof tail, "+%u", me);
      xb_set_str(&piece, xb_str_new(tail, (size_t)n));
      xb_op_add_grow(ctx, &sum, &sum, &piece);
    }
    size_t len = base_text(text, k);
    len += appended_text(text + len, me, k);
    if (!holds(&sum, text, len)) xb_atomic_add32(&bad, 1, XB_RELAXED);
    xb_array_put(results[me].v.p, k, &sum);
    poll(ctx, k);
  }
  xb_gc_root_pop(ctx, 3);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

int main(void) {
  xb_platform_init();
  int at = 0;
  xb_gc_thread_attach(&at);
  xb_ctx *ctx = xb_ctx_new();
  xb_item fill, label, base;
  xb_set_nil(&label);
  xb_set_nil(&base);
  xb_gc_root_push(ctx, &label);
  xb_gc_root_push(ctx, &base);
  char text[BASE_FILL + 128];
  memset(text, 'b', BASE_FILL);
  xb_set_str(&fill, xb_str_new(text, BASE_FILL));
  xb_gc_root_push(ctx, &fill);
  xb_set_array(&bases, xb_array_alloc(BASES));
  xb_gc_add_root(&bases, 1);
  for (uint32_t k = 0; k < BASES; ++k) {
    size_t len = base_text(text, k);
    xb_set_str(&label, xb_str_new(text + BASE_FILL, len - BASE_FILL));
    xb_op_add_grow(ctx, &base, &fill, &label); /* a view at its buffer's tip */
    CHECK(base.v.s->flags & XB_STR_VIEW);
    xb_array_put(bases.v.p, k, &base);
  }
  for (uint32_t t = 0; t < THREADS; ++t) {
    xb_set_array(&results[t], xb_array_alloc(BASES));
    xb_gc_add_root(&results[t], 1);
  }
  xb_gc_root_pop(ctx, 3);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();

  xb_thread threads[THREADS];
  for (uint32_t t = 0; t < THREADS; ++t)
    CHECK(xb_thread_start(&threads[t], (size_t)8 << 20, appender, (void *)(size_t)t));
  for (uint32_t t = 0; t < THREADS; ++t) xb_thread_join(threads[t]);
  CHECK(bad == 0);

  uint32_t tips = 0;
  for (uint32_t k = 0; k < BASES; ++k) {
    const xb_item *b = &xb_array_items(bases.v.p)[k];
    size_t len = base_text(text, k);
    CHECK(holds(b, text, len));
    uint32_t won = 0;
    for (uint32_t t = 0; t < THREADS; ++t) {
      const xb_item *r = &xb_array_items(results[t].v.p)[k];
      size_t all = len + appended_text(text + len, t, k);
      CHECK(holds(r, text, all));
      if ((r->v.s->flags & XB_STR_VIEW) && xb_str_view_buffer(r->v.s) == xb_str_view_buffer(b->v.s)) ++won;
    }
    CHECK(won == 1);
    tips += won;
  }
  CHECK(tips == BASES);
  puts("shared_strings ok");
  return 0;
}
