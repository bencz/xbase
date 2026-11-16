/* An append to a full array a root holds, the program's storage made shared by the allocation of its new items (see
 * publish.h): the array is shared, the appended value (a local array) shared with it, and it is in place. */
#include "publish.h"

#define FULL 100000 /* items large enough that their growth is a large allocation, a safepoint */

static xb_item root;

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_set_nil(&root);
  xb_gc_add_root(&root, 1);
  xb_set_array(&root, xb_array_alloc(FULL)); /* full: its length is its room */
  xb_item value;
  xb_set_array(&value, xb_array_alloc(1));
  xb_gc_root_push(ctx, &value);
  xb_thread t = publish_soon(ctx);
  xb_array *a = root.v.p;
  CHECK(!xb_is_shared(a));
  xb_array_append(ctx, a, &value);
  CHECK(xb_is_shared(a) && xb_is_shared(value.v.p));
  CHECK(xb_array_length(a) == FULL + 1);
  xb_item last;
  CHECK(xb_array_last(a, &last) && last.type == XB_T_ARRAY && last.v.p == value.v.p);
  publish_done(ctx, t);
  xb_gc_root_pop(ctx, 1);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  puts("publish_array ok");
  return 0;
}
