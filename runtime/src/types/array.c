/* Arrays: allocation, resizing and element access. Indexes are 1-based. */
#include "xbrt_int.h"

#include "core/shared.h"

#include <string.h>

/* A shared array's fields change under its lock while readers load them under its sequence counter: they are
 * stored with release and loaded with acquire order (core/shared.h). */
#if defined(__GNUC__) || defined(__clang__)
static xb_item *items_of(const xb_array *a) { return __atomic_load_n(&a->items, __ATOMIC_ACQUIRE); }
static void set_items(xb_array *a, xb_item *items) { __atomic_store_n(&a->items, items, __ATOMIC_RELEASE); }
#else
static xb_item *items_of(const xb_array *a) { return xb_atomic_loadptr((void *const *)&a->items, XB_ACQUIRE); }
static void set_items(xb_array *a, xb_item *items) { xb_atomic_storeptr((void **)&a->items, items, XB_RELEASE); }
#endif

size_t xb_array_length(const xb_array *a) {
  return xb_is_shared(a) ? xb_atomic_loadsz(&a->len, XB_ACQUIRE) : a->len;
}

int xb_array_get(const xb_array *a, size_t i, xb_item *dst) {
  if (!xb_is_shared(a)) {
    if (i >= a->len) return 0;
    *dst = a->items[i];
    return 1;
  }
  for (;;) { /* into a local item first: a read a writer overlapped must never reach `dst`, a root perhaps */
    uint32_t seq = xb_seq_read_begin(a);
    xb_item v;
    int in = i < xb_atomic_loadsz(&a->len, XB_ACQUIRE);
    if (in) xb_seq_load(&v, &items_of(a)[i]);
    if (!xb_seq_read_valid(a, seq)) continue;
    if (in) *dst = v;
    return in;
  }
}

int xb_array_last(const xb_array *a, xb_item *dst) {
  if (!xb_is_shared(a)) {
    if (!a->len) return 0;
    *dst = a->items[a->len - 1];
    return 1;
  }
  for (;;) { /* the length and the element in one read */
    uint32_t seq = xb_seq_read_begin(a);
    xb_item v;
    size_t len = xb_atomic_loadsz(&a->len, XB_ACQUIRE);
    if (len) xb_seq_load(&v, &items_of(a)[len - 1]);
    if (!xb_seq_read_valid(a, seq)) continue;
    if (len) *dst = v;
    return len != 0;
  }
}

xb_array *xb_array_snapshot(const xb_array *a) {
  if (!xb_is_shared(a)) {
    xb_array *copy = xb_array_alloc(a->len); /* `a` is reachable */
    memcpy(copy->items, a->items, a->len * sizeof(xb_item));
    return copy;
  }
  for (;;) { /* storage for the length seen, then all the items in one read of that length */
    size_t len = xb_atomic_loadsz(&a->len, XB_ACQUIRE);
    xb_array *copy = xb_array_alloc(len);
    uint32_t seq = xb_seq_read_begin(a);
    int same = xb_atomic_loadsz(&a->len, XB_ACQUIRE) == len;
    const xb_item *items = items_of(a);
    for (size_t i = 0; same && i < len; ++i) xb_seq_load(&copy->items[i], &items[i]);
    if (xb_seq_read_valid(a, seq) && same) return copy;
  }
}

int xb_array_put(xb_array *a, size_t i, const xb_item *v) {
  if (!xb_is_shared(a)) {
    if (i >= a->len) return 0;
    a->items[i] = *v;
    xb_gc_write_item(a, i);
    return 1;
  }
  xb_share(v); /* before another thread can reach it */
  xb_seq_lock(a);
  int in = i < a->len;
  if (in) {
    xb_seq_store(&a->items[i], v);
    xb_gc_write_item(a, i);
  }
  xb_seq_unlock(a);
  return in;
}

/* Storage for `cap` items, its cards clean. */
static xb_item *storage(size_t cap) {
  xb_item *items = xb_gc_alloc(XB_GC_ITEMS, cap * sizeof(xb_item) + xb_array_card_count(cap));
  memset(items + cap, 0, xb_array_card_count(cap));
  return items;
}

static size_t grown_cap(size_t cap, size_t len) {
  cap = cap ? cap * 2 : 4;
  while (cap < len) cap *= 2;
  return cap > XB_ARRAY_MAX_LEN ? XB_ARRAY_MAX_LEN : cap;
}

/* A shared array's length set to `len`. Storage large enough is allocated first, outside the lock (its capacity only
 * grows, so it is still large enough under the lock), and rooted meanwhile: the root's tag only keeps it, storage is
 * traced through its array. Under the lock the items move there and the new ones become NIL. */
static void resize_shared(xb_ctx *ctx, xb_array *a, size_t len) {
  xb_item spare, nil;
  xb_set_nil(&spare);
  xb_set_nil(&nil);
  xb_gc_root_push(ctx, &spare);
  size_t room = xb_atomic_loadsz(&a->cap, XB_ACQUIRE);
  if (len > room) {
    room = grown_cap(room, len);
    xb_set_ptr(&spare, XB_T_ARRAY, storage(room));
  }
  xb_seq_lock(a);
  if (len > a->cap) {
    xb_item *items = spare.v.p; /* where a collection while the lock was waited for left it */
    if (a->len) memcpy(items, a->items, a->len * sizeof(xb_item)); /* no other thread writes them now */
    set_items(a, items);
    xb_atomic_storesz(&a->cap, room, XB_RELEASE);
    xb_gc_write(a); /* new storage: the whole array is traced */
  }
  for (size_t i = a->len; i < len; ++i) xb_seq_store(&a->items[i], &nil);
  xb_atomic_storesz(&a->len, len, XB_RELEASE);
  xb_seq_unlock(a);
  xb_gc_root_pop(ctx, 1);
}

void xb_array_resize_any(xb_ctx *ctx, xb_array *a, uint64_t newLen) {
  if (!xb_is_shared(a)) {
    xb_array_resize(a, newLen);
    return;
  }
  if (newLen > XB_ARRAY_MAX_LEN) xb_gc_fatal("array too large");
  resize_shared(ctx, a, (size_t)newLen);
}

void xb_array_append(xb_ctx *ctx, xb_array *a, const xb_item *v) {
  if (!xb_is_shared(a)) {
    xb_array_resize(a, (uint64_t)a->len + 1);
    a->items[a->len - 1] = *v;
    xb_gc_write_item(a, a->len - 1);
    return;
  }
  xb_share(v);
  xb_item spare;
  xb_set_nil(&spare);
  xb_gc_root_push(ctx, &spare);
  size_t room = 0;
  for (;;) { /* room for one more, made outside the lock until it is enough under it (others append meanwhile) */
    size_t len = xb_atomic_loadsz(&a->len, XB_ACQUIRE), cap = xb_atomic_loadsz(&a->cap, XB_ACQUIRE);
    if (len + 1 > cap && room < len + 1) {
      if (len + 1 > XB_ARRAY_MAX_LEN) xb_gc_fatal("array too large");
      room = grown_cap(cap, len + 1);
      xb_set_ptr(&spare, XB_T_ARRAY, storage(room));
    }
    xb_seq_lock(a);
    if (a->len + 1 > a->cap) {
      if (room < a->len + 1) {
        xb_seq_unlock(a);
        continue;
      }
      xb_item *items = spare.v.p;
      memcpy(items, a->items, a->len * sizeof(xb_item));
      set_items(a, items);
      xb_atomic_storesz(&a->cap, room, XB_RELEASE);
      xb_gc_write(a);
      xb_set_nil(&spare); /* taken: the next append makes its own */
      room = 0;
    }
    xb_seq_store(&a->items[a->len], v);
    xb_atomic_storesz(&a->len, a->len + 1, XB_RELEASE);
    xb_gc_write_item(a, a->len - 1);
    xb_seq_unlock(a);
    break;
  }
  xb_gc_root_pop(ctx, 1);
}

/* AIns and ADel: the shift, in place for a local array, word by word under the lock for a shared one. */
static void shift(xb_array *a, size_t at, int up) {
  int shared = xb_is_shared(a);
  xb_item nil;
  xb_set_nil(&nil);
  if (shared) xb_seq_lock(a);
  if (at < a->len) {
    size_t last = a->len - 1, moved = last - at, hole = up ? at : last;
    if (!shared) {
      memmove(&a->items[up ? at + 1 : at], &a->items[up ? at : at + 1], moved * sizeof(xb_item));
    } else if (up) {
      for (size_t i = last; i > at; --i) xb_seq_store(&a->items[i], &a->items[i - 1]);
    } else {
      for (size_t i = at; i < last; ++i) xb_seq_store(&a->items[i], &a->items[i + 1]);
    }
    xb_seq_store(&a->items[hole], &nil);
    if (moved) xb_gc_write_items(a, up ? at + 1 : at, up ? last + 1 : last); /* young elements in other cards */
  }
  if (shared) xb_seq_unlock(a);
}

void xb_array_insert(xb_array *a, size_t at) { shift(a, at, 1); }
void xb_array_delete(xb_array *a, size_t at) { shift(a, at, 0); }

/* Grows or shrinks the visible length; new elements are NIL. */
void xb_array_resize(xb_array *a, uint64_t newLen) {
  if (newLen > XB_ARRAY_MAX_LEN) xb_gc_fatal("array too large");
  size_t len = (size_t)newLen;
  if (len > a->cap) {
    size_t cap = grown_cap(a->cap, len);
    xb_item *items = storage(cap); /* `a` is reachable */
    if (a->len) memcpy(items, a->items, a->len * sizeof(xb_item));
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

/* The value of `idx` in the hash `base` into `out`, or 0 after raising the error: for a key that cannot be one, the
 * error of an index that is not a number (whose handler may substitute the value, into `r` when given). */
static int hash_value(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx, xb_item *out) {
  if (!xb_hash_key_ok(idx)) {
    xb_item args[2] = {*base, *idx};
    xb_error(ctx, r, EG_ARG, 1068, "array access", r ? XB_ERR_SUBSTITUTE : 0, 2, args);
    return 0;
  }
  if (xb_hash_get(base->v.p, idx, out)) return 1;
  xb_array_bound_error(ctx, base, idx);
  return 0;
}

/* Compiled code reads and stores elements in place for an array and a number
 * index within it (compiler/src/codegen/FastPaths.cpp, inlineElement) and calls
 * these for every other case. */
void xb_op_index(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx) {
  if (base->type == XB_T_OBJECT && xb_class_operator(ctx, r, XB_OPERATOR_INDEX, base, idx, NULL)) return;
  if (base->type == XB_T_HASH) {
    xb_item v;
    if (hash_value(ctx, r, base, idx, &v)) *r = v;
    return;
  }
  if (base->type != XB_T_ARRAY || !xb_is_number(idx)) {
    read_position(ctx, r, base, idx); /* the error */
    return;
  }
  int64_t n = xb_num_to_i64(idx); /* read whole, whatever another thread changes meanwhile */
  if (n < 1 || (uint64_t)(n - 1) > SIZE_MAX || !xb_array_get(base->v.p, (size_t)(n - 1), r))
    xb_array_bound_error(ctx, base, idx);
}

void xb_ref_elem(xb_ctx *ctx, xb_item *r, const xb_item *base, const xb_item *idx) {
  if (base->type == XB_T_HASH) { /* the reference designates the key's entry */
    xb_item key;
    if (!xb_hash_key_ok(idx)) {
      hash_value(ctx, NULL, base, idx, &key); /* the error */
      return;
    }
    xb_set_nil(&key);
    xb_gc_root_push(ctx, &key); /* through the allocation below: another thread may remove the entry */
    if (xb_hash_lookup(base->v.p, idx, &key, NULL)) {
      xb_ref_init(r, base, 0, 0); /* `base` and `idx` are frame slots other than `r`'s */
      ((xb_ref *)r->v.p)->key = key; /* the entry's own key, as it is spelled */
    } else {
      xb_array_bound_error(ctx, base, idx);
    }
    xb_gc_root_pop(ctx, 1);
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
  xb_item ignored;
  if (base->type == XB_T_OBJECT && xb_class_operator(ctx, &ignored, XB_OPERATOR_INDEX, base, idx, value)) return;
  if (base->type == XB_T_HASH && xb_hash_key_ok(idx)) {
    xb_hash_put(ctx, base->v.p, idx, value);
    return;
  }
  if (base->type != XB_T_ARRAY || !xb_is_number(idx)) {
    xb_error(ctx, NULL, EG_ARG, 1069, "array assign", 0, 1, idx);
    return;
  }
  int64_t n = xb_num_to_i64(idx);
  if (n < 1 || (uint64_t)(n - 1) > SIZE_MAX || !xb_array_put(base->v.p, (size_t)(n - 1), value))
    xb_error(ctx, NULL, EG_BOUND, 1133, "array assign", 0, 1, idx);
}
