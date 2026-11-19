/* The barrier after storing a range of items (xb_gc_write_items) into an old array: an item that refers to a young
 * object keeps it alive through young collections also when the first item of its card refers to none (a number), in
 * an array without cards (a few items, recorded whole) and in one with cards (the range across several, the young
 * object far from its card's first item). Each case uses an array of its own, old and not recorded yet when the
 * barrier runs; the young strings are checked whole after other allocations have taken whatever memory a collection
 * freed. */
#include "check.h"
#include "xbrt_int.h"

#include <stdio.h>
#include <string.h>

#define TEXT "kept by the array's barrier"

/* A new array of `n` NIL items in `*keep`, made old: collected young until its header says so. */
static void aged(xb_item *keep, size_t n) {
  xb_set_array(keep, xb_array_alloc(n));
  for (int k = 0; k < 16 && !(xb_gc_header(keep->v.p)->state & XB_GC_MARKED); ++k) xb_gc_collect_young();
  CHECK(xb_gc_header(keep->v.p)->state & XB_GC_MARKED);
}

/* Items `first` .. `last` of a new old array of `n` stored: numbers, but `at`, a new string; the barrier for the
 * range, then young collections and other allocations; the string read back whole. */
static void check_range(xb_ctx *ctx, size_t n, int carded, size_t first, size_t last, size_t at) {
  xb_item keep, junk;
  xb_set_nil(&keep);
  xb_set_nil(&junk);
  xb_gc_root_push(ctx, &keep);
  xb_gc_root_push(ctx, &junk);
  aged(&keep, n);
  xb_array *a = keep.v.p;
  CHECK((xb_array_cards(a) != NULL) == carded);
  CHECK(!(xb_gc_header(a)->state & (XB_GC_REMEMBERED | XB_GC_CARDED))); /* the barrier alone records it */
  xb_str *s = xb_str_new(TEXT, strlen(TEXT));                          /* young; the array is old */
  for (size_t k = first; k <= last; ++k) xb_set_int(&xb_array_items(a)[k], (int64_t)k);
  xb_set_str(&xb_array_items(a)[at], s);
  xb_gc_write_items(a, first, last + 1);
  for (int round = 0; round < 3; ++round) {
    xb_gc_collect_young();
    for (int k = 0; k < 20000; ++k) xb_set_str(&junk, xb_str_new("garbage over freed memory", 25));
  }
  a = keep.v.p;
  const xb_item *it = &xb_array_items(a)[at];
  CHECK(it->type == XB_T_STRING);
  CHECK(it->v.s->len == strlen(TEXT) && memcmp(it->v.s->data, TEXT, strlen(TEXT)) == 0);
  xb_gc_root_pop(ctx, 2);
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  check_range(ctx, 6, 0, 1, 5, 3);
  check_range(ctx, 2048, 1, 64, 300, 200);
  check_range(ctx, 2048, 1, 0, 2047, 2047);
  xb_ctx_free(ctx);
  puts("gc_write_items ok");
  return 0;
}
