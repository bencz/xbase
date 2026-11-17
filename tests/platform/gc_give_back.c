/* Free blocks given back to the system after a collection: the heap's books agree after each collection (every free
 * block in use listed, none given back among those in use: a list kept through a given-back block would read memory
 * the system took back), the heap's memory comes down after a peak, and blocks are taken back and given back again in
 * many rounds. */
#include "check.h"
#include "xbrt_int.h"

#define ROUNDS 6
#define OBJECTS 400000u

int main(void) {
  xb_platform_init();
  int base;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_item keep;
  xb_set_nil(&keep);
  xb_gc_root_push(ctx, &keep);
  size_t low = 0;
  for (int round = 0; round < ROUNDS; ++round) {
    xb_array *a = xb_array_alloc(OBJECTS);
    xb_set_array(&keep, a);
    for (uint32_t k = 0; k < OBJECTS; ++k) {
      xb_str *s = xb_str_new("a block's worth of strings", 24 + k % 9);
      a = keep.v.p; /* the array may have moved */
      xb_set_str(&a->items[k], s);
      xb_gc_write_item(a, k);
    }
    size_t peak = xb_gc_heap_bytes();
    xb_set_nil(&keep);
    xb_gc_collect();
    xb_gc_collect();
    CHECK(xb_gc_consistent());
    size_t after = xb_gc_heap_bytes();
    CHECK(after < peak / 2);
    if (round == 0) low = after;
    CHECK(after < low + ((size_t)8 << 20)); /* no round keeps more than the first */
  }
  xb_gc_root_pop(ctx, 1);
  xb_ctx_free(ctx);
  puts("gc_give_back ok");
  return 0;
}
