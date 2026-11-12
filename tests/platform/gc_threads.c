/* The collector with several mutator threads: each one allocates strings and arrays, keeps some in an array rooted in
 * its context, holds the address of a new string in its own C variables (on its stack, in its registers) across
 * allocations while the string is also rooted, as the runtime's rule asks, serves safepoints as compiled code does,
 * and collects, young and full (which move objects), while the others run, park or block. In the end every kept
 * object is whole: none was freed under a thread, nor moved under one that held its address. */
#include "check.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <string.h>

#define THREADS 4
#define ROUNDS 4000u
#define KEPT 256u

static uint32_t bad;

static void pause_2ms(void *unused) {
  (void)unused;
  xb_platform_sleep_ms(2);
}

/* A string that tells its thread, round and slot, of a length from 20 to 299 bytes (lines and size classes). */
static xb_str *text(uint32_t thread, uint32_t round, uint32_t slot) {
  char buf[320];
  size_t len = 20 + (round * 7 + slot * 13) % 280;
  int n = snprintf(buf, sizeof buf, "%u:%u:%u|", thread, round, slot);
  memset(buf + n, 'a' + (int)(slot % 26), len - (size_t)n);
  return xb_str_new(buf, len);
}

static int whole(const xb_str *s, uint32_t thread, uint32_t round, uint32_t slot) {
  char head[64];
  int n = snprintf(head, sizeof head, "%u:%u:%u|", thread, round, slot);
  size_t len = 20 + (round * 7 + slot * 13) % 280;
  if (s->len != len || memcmp(s->data, head, (size_t)n) != 0) return 0;
  for (size_t k = (size_t)n; k < len; ++k)
    if (s->data[k] != 'a' + (int)(slot % 26)) return 0;
  return 1;
}

static void mutate(void *arg) {
  uint32_t me = (uint32_t)(size_t)arg;
  xb_gc_thread_attach(&arg);
  xb_ctx *ctx = xb_ctx_new();
  xb_item kept, scratch, rooted;
  xb_set_array(&kept, xb_array_alloc(KEPT));
  xb_set_nil(&scratch);
  xb_set_nil(&rooted);
  xb_gc_root_push(ctx, &kept);
  xb_gc_root_push(ctx, &scratch);
  xb_gc_root_push(ctx, &rooted);
  uint32_t keptRound[KEPT] = {0};
  for (uint32_t round = 1; round <= ROUNDS; ++round) {
    uint32_t slot = (round * 31) % KEPT;
    xb_str *held = text(me, round, slot); /* its address, held across allocations: it must not move */
    xb_set_str(&rooted, held);
    for (uint32_t k = 0; k < 8; ++k) { /* garbage around it, of every kind */
      xb_array *a = xb_array_alloc(k + 1);
      xb_set_array(&scratch, a);
      xb_set_str(&a->items[0], text(me, round, k));
      xb_gc_write_item(a, 0);
    }
    if (!whole(held, me, round, slot)) xb_atomic_add32(&bad, 1, XB_RELAXED);
    xb_array *a = kept.v.p;
    xb_set_str(&a->items[slot], held);
    xb_gc_write_item(a, slot);
    keptRound[slot] = round;
    if (xb_pending_get(ctx)) xb_pending_gc(ctx); /* a poll, as compiled code makes */
    if (round % 500 == me) xb_gc_collect();
    else if (round % 97 == me) xb_gc_collect_young();
    if (round % 400 == 0) { /* a blocking operation: collections go on without this thread */
      xb_blocking_run(pause_2ms, NULL);
    }
  }
  const xb_array *a = kept.v.p;
  for (uint32_t slot = 0; slot < KEPT; ++slot)
    if (keptRound[slot] && (a->items[slot].type != XB_T_STRING ||
                            !whole(a->items[slot].v.s, me, keptRound[slot], slot)))
      xb_atomic_add32(&bad, 1, XB_RELAXED);
  xb_gc_root_pop(ctx, 3);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

int main(void) {
  xb_platform_init();
  xb_thread threads[THREADS];
  for (size_t k = 0; k < THREADS; ++k) CHECK(xb_thread_start(&threads[k], (size_t)8 << 20, mutate, (void *)k));
  for (size_t k = 0; k < THREADS; ++k) xb_thread_join(threads[k]);
  CHECK(bad == 0);
  puts("gc_threads ok");
  return 0;
}
