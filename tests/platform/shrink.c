/* The room of the PRIVATEs and of the C roots goes back down after a deep nesting: the PRIVATEs' as a recursion that
 * makes one at each level returns (shrink.prg), the roots' (20,000 pushed, as a deep nesting of runtime calls would)
 * at the collections after; both down to their floor (1024 entries), from many times that at the peak. */
#include "check.h"
#include "xbrt_int.h"

void xbp_DEEPPRIVATES(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

#define DEPTH 20000
#define FLOOR 1024u

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_item n, r;
  xb_set_int(&n, DEPTH);
  xb_set_nil(&r);
  xb_gc_root_push(ctx, &r);
  xbp_DEEPPRIVATES(ctx, &r, 1, &n);
  CHECK(r.type == XB_T_INT && r.v.i == (int64_t)DEPTH * (DEPTH + 1) / 2);
  CHECK(ctx->env->privateLen == 0 && ctx->env->privateCap <= FLOOR);
  static xb_item held[DEPTH];
  for (int k = 0; k < DEPTH; ++k) {
    xb_set_nil(&held[k]);
    xb_gc_root_push(ctx, &held[k]);
  }
  xb_gc_root_pop(ctx, DEPTH);
  CHECK(ctx->rootCap > FLOOR); /* the peak's room, until a collection */
  for (int k = 0; k < 16 && ctx->rootCap > FLOOR; ++k) xb_gc_collect();
  CHECK(ctx->rootCap <= FLOOR && ctx->rootLen == 1);
  puts("shrink ok");
  return 0;
}
