/* Arrays: allocation, resizing and element access. Indexes are 1-based. */
#include "xbrt_int.h"

#include "core/shared.h"

#include <string.h>

/* A shared array's fields change under its lock while readers load them under its sequence counter: they are
 * stored with release and loaded with acquire order (core/shared.h). */
#if defined(__GNUC__) || defined(__clang__)
static xb_item *items_of(const xb_array *a) { return __atomic_load_n(&a->data, __ATOMIC_ACQUIRE); }
static void set_items(xb_array *a, xb_item *items) { __atomic_store_n(&a->data, (void *)items, __ATOMIC_RELEASE); }
#else
static xb_item *items_of(const xb_array *a) { return xb_atomic_loadptr((void *const *)&a->data, XB_ACQUIRE); }
static void set_items(xb_array *a, xb_item *items) { xb_atomic_storeptr((void **)&a->data, items, XB_RELEASE); }
#endif

/* The forms of the elements (xb_array in xbrt.h): a shared array holds items, so only the paths of a local one read and
 * write the others. */

/* An array's form changes only while one thread reaches it, which is then the only one to change its state byte
 * while it runs: the collector, and a publication's xb_share (which turns the arrays of the program's storage), change
 * it while that thread is stopped, and another thread records only a shared object. A load and a store, no
 * read-modify-write. */
static void set_elems(xb_array *a, uint32_t elems) {
  uint8_t *state = &xb_gc_header(a)->state;
  uint8_t was = xb_atomic_load8(state, XB_RELAXED);
  xb_atomic_store8(state, (uint8_t)((was & ~XB_GC_STATE_ELEMS) | elems << XB_GC_STATE_ELEMS_SHIFT), XB_RELAXED);
}

/* The bytes an element takes in each form. */
static size_t elem_size(uint32_t elems) {
  switch (elems) {
    case XB_ELEMS_NIL: return 0;
    case XB_ELEMS_LOGICAL: return 1;
    case XB_ELEMS_INT: return sizeof(int64_t);
    default: return sizeof(xb_item);
  }
}

/* The form an array of form `elems` needs to hold `v` too: its own when it holds it already, else the compact form of
 * v (a NIL holds every form, NIL itself turns nothing), else items. */
static uint32_t form_with(uint32_t elems, const xb_item *v) {
  uint32_t own = XB_ELEMS_ITEMS;
  if (v->width == 0 && v->dec == 0) {
    if (v->type == XB_T_NIL) return elems;
    if (v->type == XB_T_LOGICAL) own = XB_ELEMS_LOGICAL;
    if (v->type == XB_T_INT && v->v.i != XB_ELEMS_INT_NIL) own = XB_ELEMS_INT;
  }
  return elems == XB_ELEMS_NIL || elems == own ? own : XB_ELEMS_ITEMS;
}

/* Element `i` of a local array of form `elems` into *dst. */
static inline void read_elem(const xb_array *a, uint32_t elems, size_t i, xb_item *dst) {
  switch (elems) {
    case XB_ELEMS_ITEMS: *dst = ((const xb_item *)a->data)[i]; return;
    case XB_ELEMS_LOGICAL: {
      uint8_t b = ((const uint8_t *)a->data)[i];
      if (b)
        xb_set_logical(dst, b == XB_ELEMS_TRUE);
      else
        xb_set_nil(dst);
      return;
    }
    case XB_ELEMS_INT: {
      int64_t n;
      memcpy(&n, (const char *)a->data + i * sizeof n, sizeof n);
      if (n != XB_ELEMS_INT_NIL)
        xb_set_int(dst, n);
      else
        xb_set_nil(dst);
      return;
    }
    default: xb_set_nil(dst); return;
  }
}

/* The byte of an array of logicals holding `v`, a logical or NIL. */
static inline uint8_t logical_byte(const xb_item *v) {
  return v->type == XB_T_NIL ? 0 : v->v.l ? XB_ELEMS_TRUE : XB_ELEMS_FALSE;
}

/* `v` into element `i` (below the room) of a local array whose form, `elems`, holds it. */
static inline void write_elem(xb_array *a, uint32_t elems, size_t i, const xb_item *v) {
  switch (elems) {
    case XB_ELEMS_ITEMS: ((xb_item *)a->data)[i] = *v; return;
    case XB_ELEMS_LOGICAL: ((uint8_t *)a->data)[i] = logical_byte(v); return;
    case XB_ELEMS_INT: {
      int64_t n = v->type == XB_T_NIL ? XB_ELEMS_INT_NIL : v->v.i;
      memcpy((char *)a->data + i * sizeof n, &n, sizeof n);
      return;
    }
    default: return;
  }
}

/* Elements `from` .. `to` - 1 (below the room) of a local array set to NIL. */
static void clear_elems(xb_array *a, size_t from, size_t to) {
  uint32_t elems = xb_array_elems(a);
  if (elems == XB_ELEMS_INT) {
    xb_item nil;
    xb_set_nil(&nil);
    for (size_t i = from; i < to; ++i) write_elem(a, elems, i, &nil);
    return;
  }
  size_t size = elem_size(elems); /* 0 is NIL as an item and as a logical */
  if (size && to > from) memset((char *)a->data + from * size, 0, (to - from) * size);
}

/* A local array's elements in form `elems`, one that holds them all, in place: the storage has room for its items.
 * Turned into items, element i is read before the item i takes its place, from the last one down: an item covers only
 * the places of elements at or past its own. */
static void retarget(xb_array *a, uint32_t elems) {
  uint32_t was = xb_array_elems(a);
  if (was == elems) return;
  if (was == XB_ELEMS_NIL) {
    set_elems(a, elems);
    clear_elems(a, 0, a->len);
    return;
  }
  for (size_t i = a->len; i-- > 0;) {
    xb_item v;
    read_elem(a, was, i, &v);
    ((xb_item *)a->data)[i] = v;
  }
  set_elems(a, elems);
}

void xb_array_to_items(xb_array *a) { retarget(a, XB_ELEMS_ITEMS); }

/* `v` into element `i` (below the room) of a local array, which takes the form that holds it first; then the barrier,
 * which only a value that refers to an object needs. */
static inline void store(xb_array *a, size_t i, const xb_item *v) {
  uint32_t was = xb_array_elems(a);
  uint32_t form = form_with(was, v);
  if (form != was) retarget(a, form);
  write_elem(a, form, i, v);
  if (xb_type_is_heap(v->type)) xb_gc_write_item(a, i);
}

/* The state byte of an array: whether it is shared, its form. */
static inline uint8_t state_of(const xb_array *a) { return xb_atomic_load8(&xb_gc_header(a)->state, XB_RELAXED); }

size_t xb_array_length(const xb_array *a) {
  return xb_is_shared(a) ? xb_atomic_loadsz(&a->len, XB_ACQUIRE) : a->len;
}

/* The paths of a shared array are functions of their own (XB_NOINLINE): the local ones then need no frame. */
static XB_NOINLINE int get_shared(const xb_array *a, size_t i, xb_item *dst) {
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

int xb_array_get(const xb_array *a, size_t i, xb_item *dst) {
  uint8_t state = state_of(a);
  if (state & XB_GC_SHARED) return get_shared(a, i, dst);
  if (i >= a->len) return 0;
  read_elem(a, xb_elems_of_state(state), i, dst);
  return 1;
}

static XB_NOINLINE int last_shared(const xb_array *a, xb_item *dst) {
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

int xb_array_last(const xb_array *a, xb_item *dst) {
  uint8_t state = state_of(a);
  if (state & XB_GC_SHARED) return last_shared(a, dst);
  if (!a->len) return 0;
  read_elem(a, xb_elems_of_state(state), a->len - 1, dst);
  return 1;
}

/* Optimistic reads of a shared array before its copy is made under the lock. */
#define SNAPSHOT_TRIES 4

/* The copy under the lock: storage for `room` items allocated first (rooted while the lock is waited for, when
 * collections go on), then the items as the array holds them; NULL when it holds more than `room` now. */
static xb_array *copy_locked(xb_ctx *ctx, const xb_array *a, size_t room) {
  xb_item held;
  xb_set_array(&held, xb_array_alloc(room));
  xb_gc_root_push(ctx, &held);
  xb_seq_lock(a);
  size_t len = a->len;
  xb_array *copy = held.v.p;
  if (len <= room) memcpy(copy->data, a->data, len * sizeof(xb_item));
  xb_seq_unlock(a);
  xb_gc_root_pop(ctx, 1);
  if (len > room) return NULL;
  xb_gc_write(copy); /* a collection while the lock was waited for may have aged it */
  xb_array_resize(copy, len); /* fewer now: the rest are NIL */
  return copy;
}

xb_array *xb_array_snapshot(xb_ctx *ctx, const xb_array *a) {
  if (!xb_is_shared(a)) {
    xb_array *copy = xb_array_alloc(a->len); /* `a` is reachable */
    if (!xb_is_shared(a)) { /* the allocation is a safepoint, where it may have been published */
      set_elems(copy, xb_array_elems(a));
      memcpy(copy->data, a->data, a->len * elem_size(xb_array_elems(a)));
      return copy;
    }
  }
  /* Storage for the length seen, then all the items in one read of that length. A long read that writers keep
   * overtaking is made under the lock after SNAPSHOT_TRIES of them, with room for twice the length each time the
   * array grew past it meanwhile. */
  for (unsigned tries = 0;; ++tries) {
    size_t len = xb_atomic_loadsz(&a->len, XB_ACQUIRE);
    if (tries >= SNAPSHOT_TRIES) {
      for (size_t room = len;; room = room < XB_ARRAY_MAX_LEN / 2 ? 2 * room + 16 : XB_ARRAY_MAX_LEN) {
        xb_array *copy = copy_locked(ctx, a, room);
        if (copy) return copy;
      }
    }
    xb_array *copy = xb_array_alloc(len);
    uint32_t seq = xb_seq_read_begin(a);
    int same = xb_atomic_loadsz(&a->len, XB_ACQUIRE) == len;
    const xb_item *items = items_of(a);
    for (size_t i = 0; same && i < len; ++i) xb_seq_load(&((xb_item *)copy->data)[i], &items[i]);
    if (xb_seq_read_valid(a, seq) && same) return copy;
  }
}

static XB_NOINLINE int put_shared(xb_array *a, size_t i, const xb_item *v) {
  xb_share(v); /* before another thread can reach it */
  xb_seq_lock(a);
  int in = i < a->len;
  if (in) {
    xb_seq_store(&((xb_item *)a->data)[i], v);
    xb_gc_write_item(a, i);
  }
  xb_seq_unlock(a);
  return in;
}

int xb_array_put(xb_array *a, size_t i, const xb_item *v) {
  if (xb_is_shared(a)) return put_shared(a, i, v);
  if (i >= a->len) return 0;
  store(a, i, v);
  return 1;
}

/* The form that holds `v` taken once, then the elements written in one pass (a logical's bytes at once), the range's
 * barrier only for a value that refers to a heap object; a shared array's each under its lock. */
void xb_array_fill(xb_array *a, size_t from, size_t to, const xb_item *v) {
  if (xb_is_shared(a)) {
    for (size_t i = from; i < to && put_shared(a, i, v); ++i) continue;
    return;
  }
  if (to > a->len) to = a->len;
  if (from >= to) return;
  uint32_t form = form_with(xb_array_elems(a), v);
  retarget(a, form);
  if (form == XB_ELEMS_LOGICAL) {
    memset((uint8_t *)a->data + from, logical_byte(v), to - from);
    return;
  }
  for (size_t i = from; i < to; ++i) write_elem(a, form, i, v);
  if (xb_type_is_heap(v->type)) xb_gc_write_items(a, from, to);
}

/* Storage for `cap` items, its cards clean; `fallible`: NULL when there is no memory for it (xb_gc_try_alloc). */
static xb_item *storage_of(size_t cap, int fallible) {
  size_t bytes = cap * sizeof(xb_item) + xb_array_card_count(cap);
  xb_item *items = fallible ? xb_gc_try_alloc(XB_GC_ITEMS, bytes) : xb_gc_alloc(XB_GC_ITEMS, bytes);
  if (items) memset(items + cap, 0, xb_array_card_count(cap));
  return items;
}

static xb_item *storage(size_t cap) { return storage_of(cap, 0); }

static size_t grown_cap(size_t cap, size_t len) {
  cap = cap ? cap * 2 : 4;
  while (cap < len) cap *= 2;
  return cap > XB_ARRAY_MAX_LEN ? XB_ARRAY_MAX_LEN : cap;
}

/* A shared array's length set to `len`. Storage large enough is allocated first, outside the lock (its capacity only
 * grows, so it is still large enough under the lock), and rooted meanwhile: the root's tag only keeps it, storage is
 * traced through its array. Under the lock the items move there and the new ones become NIL. */
static int resize_shared(xb_ctx *ctx, xb_array *a, size_t len, int fallible) {
  xb_item spare, nil;
  xb_set_nil(&spare);
  xb_set_nil(&nil);
  size_t room = xb_atomic_loadsz(&a->cap, XB_ACQUIRE);
  if (len > room) {
    room = grown_cap(room, len);
    xb_item *items = storage_of(room, fallible);
    if (!items) return 0;
    xb_set_ptr(&spare, XB_T_ARRAY, items);
  }
  xb_gc_root_push(ctx, &spare);
  xb_seq_lock(a);
  if (len > a->cap) {
    xb_item *items = spare.v.p; /* where a collection while the lock was waited for left it */
    if (a->len) memcpy(items, a->data, a->len * sizeof(xb_item)); /* no other thread writes them now */
    set_items(a, items);
    xb_atomic_storesz(&a->cap, room, XB_RELEASE);
    xb_gc_write(a); /* new storage: the whole array is traced */
  }
  for (size_t i = a->len; i < len; ++i) xb_seq_store(&((xb_item *)a->data)[i], &nil);
  xb_atomic_storesz(&a->len, len, XB_RELEASE);
  xb_seq_unlock(a);
  xb_gc_root_pop(ctx, 1);
  return 1;
}

/* local_room past the room: a function of its own, so that appends within the room need no frame. */
static XB_NOINLINE int local_grow(xb_array *a, size_t len, int fallible) {
  size_t cap = grown_cap(a->cap, len);
  xb_item *items = storage_of(cap, fallible); /* `a` is reachable */
  if (!items) return -1; /* no memory: only when fallible */
  if (xb_is_shared(a)) return 0;
  if (a->len) memcpy(items, a->data, a->len * elem_size(xb_array_elems(a))); /* the form after the allocation */
  a->data = items;
  a->cap = cap;
  xb_gc_write(a); /* new storage: the whole array is traced */
  return 1;
}

/* Room for `len` items in a local array, made in place: the allocation is a safepoint, where the program's storage
 * may become shared (publication, docs/concurrency.md) and this array with it, so it is tested again after it: 0
 * then, nothing changed, for the shared path to take over. */
static inline int local_room(xb_array *a, size_t len, int fallible) {
  return len <= a->cap ? !xb_is_shared(a) : local_grow(a, len, fallible);
}

/* A local array shrunk to `len` items, a quarter of its room or less (of XB_ARRAY_SHRINK_MIN items at least): a peak's
 * room is given back. Items in storage of their own move into storage twice as large as they need (0 when the array
 * became shared meanwhile: the allocation is a safepoint, nothing changed); items in the array's own allocation stay,
 * its cards after them, and the collector trims the rest (a large object). */
#define XB_ARRAY_SHRINK_MIN 1024u
static int local_shrink(xb_array *a, size_t len) {
  if (a->data == a->inline_items) {
    a->len = a->cap = len;
    memset(a->inline_items + len, 0, xb_array_card_count(len)); /* clean cards; the whole array traced next */
    xb_gc_trim(a, sizeof(xb_array) + len * sizeof(xb_item) + xb_array_card_count(len));
    xb_gc_write(a);
    return 1;
  }
  size_t cap = len * 2 > 16 ? len * 2 : 16;
  xb_item *items = storage(cap); /* `a` is reachable */
  if (xb_is_shared(a)) return 0;
  memcpy(items, a->data, len * elem_size(xb_array_elems(a)));
  a->data = items;
  a->cap = cap;
  a->len = len;
  xb_gc_write(a); /* new storage: the whole array is traced */
  return 1;
}

static int resize_any(xb_ctx *ctx, xb_array *a, uint64_t newLen, int fallible) {
  if (newLen > XB_ARRAY_MAX_LEN) {
    if (fallible) return 0;
    xb_gc_fatal("array too large");
  }
  if (!xb_is_shared(a) && a->cap >= XB_ARRAY_SHRINK_MIN && newLen <= a->cap / 4 && local_shrink(a, (size_t)newLen))
    return 1;
  int room = xb_is_shared(a) ? 0 : local_room(a, (size_t)newLen, fallible);
  if (room < 0) return 0;
  if (room) {
    xb_array_resize(a, newLen); /* within its room: nothing allocates */
    return 1;
  }
  return resize_shared(ctx, a, (size_t)newLen, fallible);
}

void xb_array_resize_any(xb_ctx *ctx, xb_array *a, uint64_t newLen) { resize_any(ctx, a, newLen, 0); }

int xb_array_resize_try(xb_ctx *ctx, xb_array *a, uint64_t newLen) { return resize_any(ctx, a, newLen, 1); }

void xb_array_append(xb_ctx *ctx, xb_array *a, const xb_item *v) {
  if (!xb_is_shared(a) && local_room(a, a->len + 1, 0)) {
    store(a, a->len, v); /* within the room, past the elements its form may turn */
    a->len++;
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
      memcpy(items, a->data, a->len * sizeof(xb_item));
      set_items(a, items);
      xb_atomic_storesz(&a->cap, room, XB_RELEASE);
      xb_gc_write(a);
      xb_set_nil(&spare); /* taken: the next append makes its own */
      room = 0;
    }
    xb_seq_store(&((xb_item *)a->data)[a->len], v);
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
    xb_item *items = a->data;
    if (!shared) {
      size_t size = elem_size(xb_array_elems(a));
      char *data = a->data;
      memmove(data + (up ? at + 1 : at) * size, data + (up ? at : at + 1) * size, moved * size);
      clear_elems(a, hole, hole + 1);
    } else {
      if (up) {
        for (size_t i = last; i > at; --i) xb_seq_store(&items[i], &items[i - 1]);
      } else {
        for (size_t i = at; i < last; ++i) xb_seq_store(&items[i], &items[i + 1]);
      }
      xb_seq_store(&items[hole], &nil);
    }
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
    if (a->len) memcpy(items, a->data, a->len * elem_size(xb_array_elems(a)));
    a->data = items;
    a->cap = cap;
    xb_gc_write(a); /* new storage: the whole array is traced */
  }
  if (len > a->len) clear_elems(a, a->len, len);
  a->len = len;
}

/* A new array of `len` NILs in form `elems` (items or NIL), with room for `len` items in its own allocation. */
static xb_array *array_alloc(uint64_t newLen, int fallible, uint32_t elems) {
  if (newLen > XB_ARRAY_MAX_LEN) {
    if (fallible) return NULL;
    xb_gc_fatal("array too large");
  }
  size_t len = (size_t)newLen;
  size_t cards = xb_array_card_count(len);
  size_t bytes = sizeof(xb_array) + len * sizeof(xb_item) + cards;
  xb_array *a = fallible ? xb_gc_try_alloc(XB_GC_ARRAY, bytes) : xb_gc_alloc(XB_GC_ARRAY, bytes);
  if (!a) return NULL;
  a->len = len;
  a->cap = len;
  a->data = a->inline_items;
  if (elems == XB_ELEMS_ITEMS) { /* a new object's state byte is clear: items */
    memset(a->inline_items, 0, len * sizeof(xb_item) + cards); /* NIL items, clean cards */
  } else {
    set_elems(a, elems);
    memset(a->inline_items + len, 0, cards); /* clean cards; NILs take no bytes */
  }
  return a;
}

xb_array *xb_array_alloc(uint64_t len) { return array_alloc(len, 0, XB_ELEMS_ITEMS); }

xb_array *xb_array_try_alloc(uint64_t len, uint32_t elems) { return array_alloc(len, 1, elems); }

void xb_set_array(xb_item *it, xb_array *a) { xb_set_ptr(it, XB_T_ARRAY, a); }

/* A literal: the form that holds its elements; one of NILs alone holds items (placeholders, as the nodes of a tree:
 * read as items they need no other path), an empty one NILs (its first store gives it a form). Compiled code makes a
 * literal by the same rule from its elements' types (CodeGen::emitArrayLiteral). */
void xb_array_new(xb_ctx *ctx, xb_item *r, uint32_t n, const xb_item *elems) {
  XB_UNUSED(ctx);
  uint32_t form = XB_ELEMS_NIL;
  for (uint32_t k = 0; k < n && form != XB_ELEMS_ITEMS; ++k) form = form_with(form, &elems[k]);
  if (form == XB_ELEMS_NIL && n > 0) form = XB_ELEMS_ITEMS;
  xb_array *a = array_alloc(n, 0, form == XB_ELEMS_ITEMS ? XB_ELEMS_ITEMS : XB_ELEMS_NIL); /* elems: in a frame */
  if (form == XB_ELEMS_ITEMS) {
    if (n) memcpy(a->data, elems, n * sizeof(xb_item));
  } else {
    retarget(a, form);
    for (uint32_t k = 0; k < n; ++k) write_elem(a, form, k, &elems[k]);
  }
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
