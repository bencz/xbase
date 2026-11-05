/* References: the items that designate another item (see xbrt.h). */
#include "xbrt_int.h"

void xb_ref_init(xb_item *r, const xb_item *coll, int64_t index, uint32_t loop) {
  xb_ref *ref = xb_gc_alloc(XB_GC_REF, sizeof(xb_ref));
  ref->coll = *coll;
  ref->index = index;
  ref->loop = loop;
  xb_set_nil(&ref->value);
  xb_set_nil(r);
  r->type = XB_T_REF;
  r->v.p = ref;
}

void xb_ref_copy(xb_item *r, const xb_item *ref) {
  xb_ref *copy = xb_gc_alloc(XB_GC_REF, sizeof(xb_ref));
  *copy = *(const xb_ref *)ref->v.p;
  xb_set_nil(r);
  r->type = XB_T_REF;
  r->v.p = copy;
}

/* An array element is found again at every use, so the reference follows the
 * array when it grows or moves its items. */
static xb_item *element_target(xb_ctx *ctx, xb_ref *ref) {
  if (ref->coll.type != XB_T_ARRAY) return &ref->value;
  xb_array *a = ref->coll.v.p;
  if (ref->index < 1 || (uint64_t)ref->index > a->len) {
    xb_array_bound_error(ctx);
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
  if (ref->coll.type == XB_T_ARRAY && ref->index >= 1 && (uint64_t)ref->index <= ((xb_array *)ref->coll.v.p)->len)
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
  xb_set_nil(r);
  r->type = XB_T_CELL;
  r->v.p = cell;
}
