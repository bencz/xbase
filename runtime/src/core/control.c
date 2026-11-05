/* Runtime support of FOR EACH and SWITCH. */
#include "xbrt_int.h"

#include <string.h>

/* FOR EACH iterates arrays (their items) and strings (one-character strings);
 * indexes are 1-based. */
int64_t xb_enum_len(xb_ctx *ctx, const xb_item *coll) {
  if (coll->type == XB_T_ARRAY) return (int64_t)((const xb_array *)coll->v.p)->len;
  if (coll->type == XB_T_STRING) return (int64_t)coll->v.s->len;
  xb_rt_error(ctx, "BASE", 1068, "Argument error", "array access");
  return -1;
}

void xb_enum_ref_new(xb_ctx *ctx, xb_item *r, const xb_item *coll) {
  XB_UNUSED(ctx);
  xb_ref *ref = xb_gc_alloc(XB_GC_REF, sizeof(xb_ref)); /* `coll` is a frame slot: still rooted */
  ref->coll = *coll;
  ref->index = 0;
  xb_set_nil(&ref->value);
  xb_set_nil(r);
  r->type = XB_T_REF;
  r->v.p = ref;
}

void xb_enum_ref_move(xb_ctx *ctx, xb_item *r, int64_t idx) {
  xb_ref *ref = r->v.p;
  ref->index = idx;
  if (ref->coll.type == XB_T_STRING) {
    const xb_str *s = ref->coll.v.s;
    if (idx >= 1 && (uint64_t)idx <= s->len) xb_set_str(&ref->value, xb_str_new(s->data + idx - 1, 1));
  } else {
    xb_ref_target(ctx, r); /* an array that shrank under the loop: bound error */
  }
}

/* An array element is found again at every use, so the reference follows the
 * array when it grows or moves its items. */
xb_item *xb_ref_target(xb_ctx *ctx, xb_item *r) {
  xb_ref *ref = r->v.p;
  if (ref->coll.type != XB_T_ARRAY) return &ref->value;
  xb_array *a = ref->coll.v.p;
  if (ref->index < 1 || (uint64_t)ref->index > a->len) {
    xb_array_bound_error(ctx);
    return &ref->value;
  }
  return &a->items[ref->index - 1];
}

/* SWITCH compares integers and strings only. */
void xb_switch_check(xb_ctx *ctx, const xb_item *value) {
  if (value->type != XB_T_INT && value->type != XB_T_STRING)
    xb_rt_error(ctx, "BASE", 3104, "Argument error", "SWITCH");
}

int32_t xb_switch_match(const xb_item *value, const xb_item *c) {
  if (value->type != c->type) return 0;
  if (value->type == XB_T_INT) return value->v.i == c->v.i;
  const xb_str *a = value->v.s, *b = c->v.s;
  return a->len == b->len && memcmp(a->data, b->data, a->len) == 0;
}
