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

void xb_ref_copy(xb_item *r, const xb_item *ref) {
  xb_ref *copy = xb_gc_alloc(XB_GC_REF, sizeof(xb_ref));
  const xb_ref *from = ref->v.p; /* a loop's, shared once the STATIC that holds it is: its index and item read whole */
  *copy = *from;
  copy->index = xb_ref_index(from);
  xb_ref_own_get(from, &copy->value);
  if (copy->coll.type == XB_T_HASH && copy->key.type == XB_T_NIL) /* the current entry, by its key from now on */
    xb_hash_entry(copy->coll.v.p, copy->index, &copy->key, NULL); /* `copy` is new: no other thread reaches it */
  xb_set_ptr(r, XB_T_REF, copy);
}

void xb_ref_of(xb_ctx *ctx, xb_item *r, xb_item *var, uint32_t is_static) {
  /* `var` is a root: the copy and the cell are made while it is reachable */
  if (!is_static && xb_is_loop_ref(var)) {
    xb_ref_copy(r, var); /* the current element: the variable stays the loop's */
    return;
  }
  xb_cell *cell = xb_cell_move(ctx, var, is_static);
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

/* A STATIC's own item (xb_ref_load and xb_ref_store with XB_T_NIL): read whole under the stripe of its address once the
 * program's storage is shared, written there under its lock, a value stored shared first; through the cell or the
 * element it holds (rooted: another thread may replace a loop's reference meanwhile). */
static int holds_ref(const xb_item *it) { return it->type == XB_T_CELL || it->type == XB_T_REF; }

static void storage_load(xb_ctx *ctx, const xb_item *var, xb_item *dst) {
  xb_item own;
  if (xb_storage_is_shared()) xb_seq_get(var, &own, var);
  else own = *var;
  if (!holds_ref(&own)) {
    *dst = own;
    return;
  }
  xb_gc_root_push(ctx, &own);
  xb_ref_get(ctx, &own, dst);
  xb_gc_root_pop(ctx, 1);
}

static void storage_store(xb_ctx *ctx, xb_item *var, const xb_item *v) {
  xb_item own;
  if (!xb_storage_is_shared()) {
    own = *var;
    if (!holds_ref(&own)) {
      *var = *v; /* STATIC storage is a root: no barrier */
      return;
    }
  } else {
    xb_share(v);
    xb_seq_lock(var);
    own = *var;
    if (!holds_ref(&own)) xb_seq_store(var, v);
    xb_seq_unlock(var);
    if (!holds_ref(&own)) return;
  }
  xb_gc_root_push(ctx, &own);
  xb_ref_set(ctx, &own, v);
  xb_gc_root_pop(ctx, 1);
}

void xb_ref_load(xb_ctx *ctx, uint32_t type, void *target, xb_item *dst) {
  if (type == XB_T_NIL) {
    storage_load(ctx, target, dst);
    return;
  }
  const xb_ref *ref = target;
  if (type == XB_T_REF && !xb_is_shared(ref)) { /* the usual one, a FOR EACH variable's element: local, in place */
    if (ref->coll.type == XB_T_ARRAY) {
      const xb_array *a = ref->coll.v.p;
      if (!xb_is_shared(a) && ref->index >= 1 && (uint64_t)ref->index <= a->len) {
        xb_array_get(a, (size_t)(ref->index - 1), dst); /* in its form: a read turns nothing */
        return;
      }
    } else if (ref->coll.type != XB_T_HASH) {
      *dst = ref->value; /* a string's character, an enumerator's Current */
      return;
    }
  }
  xb_item r; /* the cell or reference: the variable that holds it keeps it */
  xb_set_ptr(&r, (xb_type)type, target);
  xb_ref_get(ctx, &r, dst);
}

void xb_ref_store(xb_ctx *ctx, uint32_t type, void *target, const xb_item *v) {
  if (type == XB_T_NIL) {
    storage_store(ctx, target, v);
    return;
  }
  xb_item r;
  xb_set_ptr(&r, (xb_type)type, target);
  xb_ref_set(ctx, &r, v);
}
