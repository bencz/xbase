/* Arrays: allocation, resizing and element access. Indexes are 1-based. */
#include "xbrt_int.h"

#include <string.h>

/* Grows or shrinks the visible length; new elements are NIL. */
void xb_array_resize(xb_array *a, size_t len) {
  if (len > a->cap) {
    if (len > XB_ARRAY_MAX_LEN) xb_gc_fatal("array too large");
    size_t cap = a->cap ? a->cap * 2 : 4;
    while (cap < len) cap *= 2;
    if (cap > XB_ARRAY_MAX_LEN) cap = XB_ARRAY_MAX_LEN;
    xb_item *items = xb_gc_alloc(XB_GC_ITEMS, cap * sizeof(xb_item)); /* `a` is reachable */
    if (a->len) memcpy(items, a->items, a->len * sizeof(xb_item));
    a->items = items;
    a->cap = cap;
  }
  if (len > a->len) memset(a->items + a->len, 0, (len - a->len) * sizeof(xb_item)); /* NIL */
  a->len = len;
}

/* A new array of `len` NIL items, with room for `len` items in its own allocation. */
xb_array *xb_array_alloc(size_t len) {
  if (len > XB_ARRAY_MAX_LEN) xb_gc_fatal("array too large");
  xb_array *a = xb_gc_alloc(XB_GC_ARRAY, sizeof(xb_array) + len * sizeof(xb_item));
  a->len = len;
  a->cap = len;
  a->items = a->inline_items;
  if (len) memset(a->items, 0, len * sizeof(xb_item));
  return a;
}

void xb_set_array(xb_item *it, xb_array *a) {
  xb_set_nil(it);
  it->type = XB_T_ARRAY;
  it->v.p = a;
}

void xb_array_new(xb_ctx *ctx, xb_item *r, uint32_t n, const xb_item *elems) {
  XB_UNUSED(ctx);
  xb_array *a = xb_array_alloc(n); /* elems live in the caller's frame: still reachable */
  if (n) memcpy(a->items, elems, n * sizeof(xb_item));
  xb_set_array(r, a);
}

/* Index of a 1-based numeric position in `a`, or -1. */
static int64_t position(const xb_array *a, const xb_item *idx) {
  if (!xb_is_number(idx)) return -1;
  int64_t i = xb_num_to_i64(idx);
  return i >= 1 && (uint64_t)i <= a->len ? i - 1 : -1;
}

void xb_op_index(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx) {
  if (base->type != XB_T_ARRAY || !xb_is_number(idx)) {
    xb_rt_error(ctx, "BASE", 1068, "Argument error", "array access");
    return;
  }
  const xb_array *a = base->v.p;
  int64_t i = position(a, idx);
  if (i < 0) {
    xb_rt_error(ctx, "BASE", 1132, "Bound error", "array access");
    return;
  }
  *r = a->items[i];
}

void xb_op_index_set(xb_ctx *ctx, const xb_item *base, const xb_item *idx, const xb_item *value) {
  if (base->type != XB_T_ARRAY || !xb_is_number(idx)) {
    xb_rt_error(ctx, "BASE", 1069, "Argument error", "array assign");
    return;
  }
  xb_array *a = base->v.p;
  int64_t i = position(a, idx);
  if (i < 0) {
    xb_rt_error(ctx, "BASE", 1133, "Bound error", "array assign");
    return;
  }
  a->items[i] = *value;
}
