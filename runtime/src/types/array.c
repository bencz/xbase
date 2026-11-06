/* Arrays: allocation, resizing and element access. Indexes are 1-based. */
#include "xbrt_int.h"

#include <string.h>

/* Grows or shrinks the visible length; new elements are NIL. */
void xb_array_resize(xb_array *a, uint64_t newLen) {
  if (newLen > XB_ARRAY_MAX_LEN) xb_gc_fatal("array too large");
  size_t len = (size_t)newLen;
  if (len > a->cap) {
    size_t cap = a->cap ? a->cap * 2 : 4;
    while (cap < len) cap *= 2;
    if (cap > XB_ARRAY_MAX_LEN) cap = XB_ARRAY_MAX_LEN;
    size_t cards = xb_array_card_count(cap);
    xb_item *items = xb_gc_alloc(XB_GC_ITEMS, cap * sizeof(xb_item) + cards); /* `a` is reachable */
    if (a->len) memcpy(items, a->items, a->len * sizeof(xb_item));
    memset(items + cap, 0, cards);
    a->items = items;
    a->cap = cap;
    xb_gc_write(a); /* new storage: the whole array is traced */
  }
  if (len > a->len) memset(a->items + a->len, 0, (len - a->len) * sizeof(xb_item)); /* NIL */
  a->len = len;
}

/* A new array of `len` NIL items, with room for `len` items in its own allocation. */
xb_array *xb_array_alloc(uint64_t newLen) {
  if (newLen > XB_ARRAY_MAX_LEN) xb_gc_fatal("array too large");
  size_t len = (size_t)newLen;
  size_t cards = xb_array_card_count(len);
  xb_array *a = xb_gc_alloc(XB_GC_ARRAY, sizeof(xb_array) + len * sizeof(xb_item) + cards);
  a->len = len;
  a->cap = len;
  a->items = a->inline_items;
  memset(a->items, 0, len * sizeof(xb_item) + cards); /* NIL items, clean cards */
  return a;
}

void xb_set_array(xb_item *it, xb_array *a) { xb_set_ptr(it, XB_T_ARRAY, a); }

void xb_array_new(xb_ctx *ctx, xb_item *r, uint32_t n, const xb_item *elems) {
  XB_UNUSED(ctx);
  xb_array *a = xb_array_alloc(n); /* elems live in the caller's frame: still reachable */
  if (n) memcpy(a->items, elems, n * sizeof(xb_item));
  xb_set_array(r, a);
}

/* Index of a 1-based numeric position in `a`, or -1. */
static int64_t position(const xb_array *a, const xb_item *idx) {
  return xb_is_number(idx) ? xb_array_index(a, xb_num_to_i64(idx)) : -1;
}

/* The 0-based position base[idx] reads, or -1 after raising the error. The
 * handler of the error for an index that is not a number may substitute the
 * element's value, into `r` when given. */
static int64_t read_position(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx) {
  if (base->type != XB_T_ARRAY || !xb_is_number(idx)) {
    xb_item args[2] = {*base, *idx};
    xb_error(ctx, r, EG_ARG, 1068, "array access", r && !xb_is_number(idx) ? XB_ERR_SUBSTITUTE : 0, 2, args);
    return -1;
  }
  int64_t i = position(base->v.p, idx);
  if (i < 0) xb_array_bound_error(ctx, base, idx);
  return i;
}

/* The value of `idx` in the hash `base`, or NULL after raising the error:
 * for a key that cannot be one, the error of an index that is not a number
 * (whose handler may substitute the value, into `r` when given). */
static xb_item *hash_value(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx) {
  xb_hash *h = base->v.p;
  if (!xb_hash_key_ok(idx)) {
    xb_item args[2] = {*base, *idx};
    xb_error(ctx, r, EG_ARG, 1068, "array access", r ? XB_ERR_SUBSTITUTE : 0, 2, args);
    return NULL;
  }
  int64_t e = xb_hash_find(h, idx);
  if (e < 0) {
    xb_array_bound_error(ctx, base, idx);
    return NULL;
  }
  return &h->entries[e].value;
}

/* Compiled code reads and stores elements in place for an array and a number
 * index within it (compiler/src/codegen/FastPaths.cpp, inlineElement) and calls
 * these for every other case. */
void xb_op_index(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx) {
  if (base->type == XB_T_HASH) {
    const xb_item *v = hash_value(ctx, r, base, idx);
    if (v) *r = *v;
    return;
  }
  int64_t i = read_position(ctx, r, base, idx);
  if (i >= 0) *r = ((const xb_array *)base->v.p)->items[i];
}

void xb_ref_elem(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx) {
  if (base->type == XB_T_HASH) { /* the reference designates the key's entry */
    if (!hash_value(ctx, NULL, base, idx)) return;
    xb_ref_init(r, base, 0, 0); /* `base` and `idx` are frame slots other than `r`'s */
    xb_hash *h = base->v.p;
    ((xb_ref *)r->v.p)->key = h->entries[xb_hash_find(h, idx)].key; /* the entry's own key, as it is spelled */
    return;
  }
  int64_t i = read_position(ctx, NULL, base, idx);
  if (i >= 0) xb_ref_init(r, base, i + 1, 0);
}

void xb_array_bound_error(xb_ctx *ctx, const xb_item *base, const xb_item *idx) {
  xb_item args[2] = {*base, *idx};
  xb_error(ctx, NULL, EG_BOUND, 1132, "array access", 0, 2, args);
}

void xb_op_index_set(xb_ctx *ctx, const xb_item *base, const xb_item *idx, const xb_item *value) {
  if (base->type == XB_T_HASH && xb_hash_key_ok(idx)) {
    xb_hash_set(base->v.p, idx, value);
    return;
  }
  if (base->type != XB_T_ARRAY || !xb_is_number(idx)) {
    xb_error(ctx, NULL, EG_ARG, 1069, "array assign", 0, 1, idx);
    return;
  }
  xb_array *a = base->v.p;
  int64_t i = position(a, idx);
  if (i < 0) {
    xb_error(ctx, NULL, EG_BOUND, 1133, "array assign", 0, 1, idx);
    return;
  }
  a->items[i] = *value;
  xb_gc_write_item(a, (size_t)i);
}
