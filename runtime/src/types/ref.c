/* References: the items that designate another item (see xbrt.h). */
#include "xbrt_int.h"

#include "core/shared.h"

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
  if (copy->coll.type == XB_T_HASH && copy->key.type == XB_T_NIL) /* the current entry, by its key from now on */
    xb_hash_entry(copy->coll.v.p, copy->index, &copy->key, NULL); /* `copy` is new: no other thread reaches it */
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

/* A reference's index and own item when another thread may reach it (a loop's, in a shared cell): the index, one
 * word, read and written atomically; the item whole, under the reference's counter or lock, a value stored shared
 * first. */
int64_t xb_ref_index(const xb_ref *ref) {
  return xb_is_shared(ref) ? (int64_t)xb_atomic_load64((const uint64_t *)&ref->index, XB_RELAXED) : ref->index;
}

void xb_ref_set_index(xb_ref *ref, int64_t index) {
  if (xb_is_shared(ref)) xb_atomic_store64((uint64_t *)&ref->index, (uint64_t)index, XB_RELAXED);
  else ref->index = index;
}

void xb_ref_own_get(const xb_ref *ref, xb_item *dst) {
  if (xb_is_shared(ref)) xb_seq_get(ref, dst, &ref->value);
  else *dst = ref->value;
}

void xb_ref_own_put(xb_ref *ref, const xb_item *v) {
  if (xb_is_shared(ref)) {
    xb_share(v);
    xb_seq_put(ref, &ref->value, v);
  } else {
    ref->value = *v;
  }
  xb_gc_write(ref);
}

/* The bound error of an element a reference designates that is gone. */
static void gone(xb_ctx *ctx, const xb_ref *ref) {
  if (ref->coll.type == XB_T_HASH && ref->key.type != XB_T_NIL) {
    xb_array_bound_error(ctx, &ref->coll, &ref->key);
    return;
  }
  xb_item idx;
  xb_set_int(&idx, xb_ref_index(ref));
  xb_array_bound_error(ctx, &ref->coll, &idx);
}

/* The collection, the key and the loop are set when the reference is made, before another thread may reach it. */
static void element_get(xb_ctx *ctx, const xb_ref *ref, xb_item *dst) {
  int found = 1;
  int64_t index = xb_ref_index(ref);
  if (ref->coll.type == XB_T_HASH)
    found = ref->key.type == XB_T_NIL ? xb_hash_entry(ref->coll.v.p, index, NULL, dst)
                                      : xb_hash_get(ref->coll.v.p, &ref->key, dst);
  else if (ref->coll.type == XB_T_ARRAY)
    found = index >= 1 && (uint64_t)(index - 1) <= SIZE_MAX && xb_array_get(ref->coll.v.p, (size_t)(index - 1), dst);
  if (found && (ref->coll.type == XB_T_HASH || ref->coll.type == XB_T_ARRAY)) return;
  if (!found) gone(ctx, ref);
  xb_ref_own_get(ref, dst);
}

static void element_set(xb_ctx *ctx, xb_ref *ref, const xb_item *v) {
  int found = 1;
  int64_t index = xb_ref_index(ref);
  if (ref->coll.type == XB_T_HASH)
    found = ref->key.type == XB_T_NIL ? xb_hash_value_at(ref->coll.v.p, index, v, NULL)
                                      : xb_hash_replace(ref->coll.v.p, &ref->key, v);
  else if (ref->coll.type == XB_T_ARRAY)
    found = index >= 1 && (uint64_t)(index - 1) <= SIZE_MAX && xb_array_put(ref->coll.v.p, (size_t)(index - 1), v);
  if (found && (ref->coll.type == XB_T_HASH || ref->coll.type == XB_T_ARRAY)) return;
  if (!found) gone(ctx, ref);
  if (!(xb_pending_get(ctx) & XB_PENDING_UNWIND)) xb_ref_own_put(ref, v); /* a string's character, or a substitute */
}

void xb_ref_get(xb_ctx *ctx, const xb_item *r, xb_item *dst) {
  if (r->type != XB_T_CELL) {
    element_get(ctx, r->v.p, dst);
    return;
  }
  xb_item v; /* rooted: another thread may put something else in the cell while a lock is waited for */
  xb_cell_get(r->v.p, &v);
  if (v.type != XB_T_REF) {
    *dst = v;
    return;
  }
  xb_gc_root_push(ctx, &v);
  element_get(ctx, v.v.p, dst);
  xb_gc_root_pop(ctx, 1);
}

void xb_ref_set(xb_ctx *ctx, const xb_item *r, const xb_item *v) {
  if (r->type != XB_T_CELL) {
    element_set(ctx, r->v.p, v);
    return;
  }
  xb_item cur; /* rooted, as in xb_ref_get */
  xb_cell_get(r->v.p, &cur);
  if (cur.type != XB_T_REF) {
    xb_cell_put(r->v.p, v);
    return;
  }
  xb_gc_root_push(ctx, &cur);
  element_set(ctx, cur.v.p, v);
  xb_gc_root_pop(ctx, 1);
}
