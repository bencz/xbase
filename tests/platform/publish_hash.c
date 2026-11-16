/* A new key in a full hash a root holds, the program's storage made shared by the allocation of its new store (see
 * publish.h): the hash is shared, the value stored (a local array) shared with it, and every key is there. */
#include "publish.h"

#define FULL 16384u /* a capacity whose growth is a large allocation, a safepoint */

static xb_item root;

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_set_nil(&root);
  xb_gc_add_root(&root, 1);
  xb_set_hash(&root, xb_hash_alloc());
  xb_hash *h = root.v.p;
  xb_item key, value;
  for (uint32_t k = 1; k <= FULL; ++k) { /* full: no room for one more, no holes */
    xb_set_int(&key, k);
    xb_hash_set(h, &key, &key);
  }
  xb_set_array(&value, xb_array_alloc(1));
  xb_gc_root_push(ctx, &value);
  xb_thread t = publish_soon(ctx);
  CHECK(!xb_is_shared(h));
  xb_set_int(&key, FULL + 1);
  xb_hash_put(ctx, h, &key, &value);
  CHECK(xb_is_shared(h) && xb_is_shared(value.v.p));
  CHECK(xb_hash_length(h) == FULL + 1);
  xb_item got;
  for (uint32_t k = 1; k <= FULL + 1; ++k) {
    xb_set_int(&key, k);
    CHECK(xb_hash_get(h, &key, &got));
  }
  CHECK(got.type == XB_T_ARRAY && got.v.p == value.v.p);
  publish_done(ctx, t);
  xb_gc_root_pop(ctx, 1);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  puts("publish_hash ok");
  return 0;
}
