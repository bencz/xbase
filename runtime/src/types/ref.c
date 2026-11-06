/* References: the items that designate another item (see xbrt.h). */
#include "xbrt_int.h"

void xb_ref_init(xb_item *r, const xb_item *coll, int64_t index, uint32_t loop) {
  xb_ref *ref = xb_gc_alloc(XB_GC_REF, sizeof(xb_ref));
  ref->coll = *coll;
  ref->index = index;
  ref->loop = loop;
  xb_set_nil(&ref->value);
  xb_set_nil(&ref->key);
  xb_set_ptr(r, XB_T_REF, ref);
}

/* The hash entry a reference designates, or NULL when it is gone. */
static xb_hentry *hash_entry(const xb_ref *ref) {
  xb_hash *h = ref->coll.v.p;
  if (ref->key.type == XB_T_NIL) return xb_hash_at(h, ref->index);
  int64_t e = xb_hash_find(h, &ref->key);
  return e < 0 ? NULL : &h->entries[e];
}

void xb_ref_copy(xb_item *r, const xb_item *ref) {
  xb_ref *copy = xb_gc_alloc(XB_GC_REF, sizeof(xb_ref));
  *copy = *(const xb_ref *)ref->v.p;
  if (copy->coll.type == XB_T_HASH && copy->key.type == XB_T_NIL) { /* the current entry, by its key from now on */
    const xb_hentry *e = hash_entry(copy);
    if (e) copy->key = e->key;
  }
  xb_set_ptr(r, XB_T_REF, copy);
}

/* An array element is found again at every use, so the reference follows the
 * array when it grows or moves its items; a hash entry is found by its key or
 * position. */
static xb_item *element_target(xb_ctx *ctx, xb_ref *ref) {
  if (ref->coll.type == XB_T_HASH) {
    xb_hentry *e = hash_entry(ref);
    if (e) return &e->value;
    if (ref->key.type != XB_T_NIL) {
      xb_array_bound_error(ctx, &ref->coll, &ref->key);
    } else {
      xb_item idx;
      xb_set_int(&idx, ref->index);
      xb_array_bound_error(ctx, &ref->coll, &idx);
    }
    return &ref->value;
  }
  if (ref->coll.type != XB_T_ARRAY) return &ref->value;
  xb_array *a = ref->coll.v.p;
  if (ref->index < 1 || (uint64_t)ref->index > a->len) {
    xb_item idx;
    xb_set_int(&idx, ref->index);
    xb_array_bound_error(ctx, &ref->coll, &idx);
    return &ref->value;
  }
  return &a->items[ref->index - 1];
}

xb_item *xb_ref_target(xb_ctx *ctx, xb_item *r) {
  if (r->type == XB_T_CELL) {
    r = &((xb_cell *)r->v.p)->value;
    if (r->type != XB_T_REF) return r;
  }
  return element_target(ctx, r->v.p);
}

/* The object holding the target: the cell, the array, or the reference
 * itself for a string's character. */
void xb_ref_written(const xb_item *r) {
  if (r->type == XB_T_CELL) {
    xb_cell *cell = r->v.p;
    if (cell->value.type != XB_T_REF) {
      xb_gc_write(cell);
      return;
    }
    r = &cell->value;
  }
  xb_ref *ref = r->v.p;
  int in_array = ref->coll.type == XB_T_ARRAY && ref->index >= 1 &&
                 (uint64_t)ref->index <= ((xb_array *)ref->coll.v.p)->len;
  if (ref->coll.type == XB_T_HASH && hash_entry(ref))
    xb_gc_write(ref->coll.v.p); /* the hash traces its entries */
  else if (in_array)
    xb_gc_write_item(ref->coll.v.p, (size_t)(ref->index - 1));
  else
    xb_gc_write(ref); /* the store went to the reference's own item */
}

void xb_ref_of(xb_ctx *ctx, xb_item *r, xb_item *var, uint32_t is_static) {
  XB_UNUSED(ctx);
  /* `var` is a root: the copy and the cell are made while it is reachable */
  if (!is_static && xb_is_loop_ref(var)) {
    xb_ref_copy(r, var); /* the current element: the variable stays the loop's */
    return;
  }
  xb_cell *cell = xb_cell_move(var);
  xb_set_ptr(r, XB_T_CELL, cell);
}
