#include "rtl/collections.h"

#include <stdlib.h>
#include <string.h>

uint64_t xb_range_start(uint32_t argc, const xb_item *argv, uint32_t k, size_t len) {
  uint64_t start = 1;
  if (argc > k && xb_is_number(&argv[k])) {
    int64_t v = xb_num_to_i64(&argv[k]);
    if (v < 0) return 0; /* a negative start is past any end */
    if (v > 0) start = (uint64_t)v;
  }
  return start > len ? 0 : start;
}

uint64_t xb_range_count(uint32_t argc, const xb_item *argv, uint32_t k, uint64_t rest) {
  if (argc <= k || !xb_is_number(&argv[k])) return rest;
  int64_t v = xb_num_to_i64(&argv[k]);
  if (v < 0) return rest;
  return (uint64_t)v < rest ? (uint64_t)v : rest;
}

void xb_calls_begin(xb_ctx *ctx, xb_item *call, uint32_t n, xb_frame *frame, const xb_proc *proc) {
  for (uint32_t k = 0; k < n; ++k) {
    xb_set_nil(&call[k]);
    xb_gc_root_push(ctx, &call[k]);
  }
  xb_frame_push(ctx, frame, proc);
}

void xb_calls_end(xb_ctx *ctx, const xb_frame *frame, uint32_t n) {
  xb_frame_pop(ctx, frame);
  xb_gc_root_pop(ctx, n);
}

int xb_calls_run(xb_ctx *ctx, const xb_item *block, xb_item *call, uint32_t nargs) {
  xb_block_call(ctx, &call[nargs], block, nargs, call);
  return !(ctx->pending && xb_pending(ctx));
}

int xb_order_answer(const xb_item *r) {
  if (r->type == XB_T_LOGICAL) return r->v.l != 0;
  if (xb_is_number(r)) return xb_num_value(r) != 0.0;
  return 1;
}

static int value_kind(const xb_item *it) {
  switch ((xb_type)it->type) {
  case XB_T_ARRAY: case XB_T_OBJECT: case XB_T_HASH: return 1;
  case XB_T_BLOCK: return 2;
  case XB_T_STRING: return 3;
  case XB_T_LOGICAL: return 4;
  case XB_T_DATE: return 5;
  case XB_T_INT: case XB_T_DOUBLE: return 6;
  default: return 7;
  }
}

int xb_value_less(const xb_ctx *ctx, const xb_item *x, const xb_item *y) {
  int c = xb_compare(ctx, x, y);
  return c == 2 ? value_kind(x) < value_kind(y) : c < 0;
}

int xb_value_matches(const xb_ctx *ctx, const xb_item *x, const xb_item *search) {
  if (x->type == XB_T_NIL || search->type == XB_T_NIL) return x->type == search->type;
  return xb_compare(ctx, x, search) == 0;
}

void xb_merge_sort(size_t *pos, size_t *tmp, size_t n, xb_less_fn less, void *arg) {
  if (n < 2) return;
  size_t left = n / 2;
  xb_merge_sort(pos, tmp, left, less, arg);
  xb_merge_sort(pos + left, tmp, n - left, less, arg);
  size_t i = 0, j = left, k = 0;
  while (i < left && j < n) tmp[k++] = less(arg, pos[j], pos[i]) ? pos[j++] : pos[i++];
  while (i < left) tmp[k++] = pos[i++];
  while (j < n) tmp[k++] = pos[j++];
  memcpy(pos, tmp, n * sizeof *pos);
}

typedef struct clone_entry {
  const void *src;
  void *copy;
} clone_entry;

typedef struct clone_visit {
  const void *src;
  xb_item copy;
} clone_visit;

typedef struct clone_map { /* open addressing, power-of-two size */
  clone_entry *slots;
  size_t mask, count;
  clone_visit *order; /* the copies to visit, in creation order */
  size_t orderLen, orderCap;
} clone_map;

static size_t clone_slot(const clone_map *m, const void *src) {
  size_t h = (size_t)(((uintptr_t)src >> 4) * (uintptr_t)UINT64_C(0x9E3779B97F4A7C15));
  for (size_t i = h & m->mask;; i = (i + 1) & m->mask)
    if (!m->slots[i].src || m->slots[i].src == src) return i;
}

static void clone_add(clone_map *m, const void *src, const xb_item *copy) {
  if ((m->count + 1) * 2 > m->mask + 1) {
    clone_map grown = *m;
    grown.mask = m->mask * 2 + 1;
    grown.slots = calloc(grown.mask + 1, sizeof(clone_entry));
    if (!grown.slots) xb_gc_fatal("out of memory");
    for (size_t i = 0; i <= m->mask; ++i)
      if (m->slots[i].src) grown.slots[clone_slot(&grown, m->slots[i].src)] = m->slots[i];
    free(m->slots);
    *m = grown;
  }
  m->slots[clone_slot(m, src)] = (clone_entry){src, copy->v.p};
  ++m->count;
  if (m->orderLen == m->orderCap) {
    m->orderCap = m->orderCap ? m->orderCap * 2 : 16;
    m->order = realloc(m->order, m->orderCap * sizeof *m->order);
    if (!m->order) xb_gc_fatal("out of memory");
  }
  m->order[m->orderLen++] = (clone_visit){src, *copy};
}

/* Replaces the array or hash in `it`, a slot of a reachable object, by its
 * copy (a new hash stays empty until visited); 0 for other values. Sources
 * stay reachable from the original. */
static int clone_item(clone_map *m, xb_item *it) {
  if (it->type != XB_T_ARRAY && it->type != XB_T_HASH) return 0;
  const void *src = it->v.p;
  const clone_entry *e = &m->slots[clone_slot(m, src)];
  if (e->src) {
    it->v.p = e->copy;
    return 1;
  }
  if (it->type == XB_T_ARRAY) {
    const xb_array *a = src;
    xb_array *copy = xb_array_alloc(a->len);
    memcpy(copy->items, a->items, a->len * sizeof(xb_item));
    xb_set_array(it, copy);
  } else {
    xb_set_hash(it, xb_hash_alloc());
  }
  clone_add(m, src, it);
  return 1;
}

void xb_clone_deep(xb_item *ret, const xb_item *root) {
  clone_map m = {calloc(16, sizeof(clone_entry)), 15, 0, NULL, 0, 0};
  if (!m.slots) xb_gc_fatal("out of memory");
  *ret = *root;
  clone_item(&m, ret); /* every copy below hangs from `ret` once made */
  for (size_t next = 0; next < m.orderLen; ++next) {
    clone_visit v = m.order[next];
    if (v.copy.type == XB_T_HASH) {
      xb_hash *h = v.copy.v.p;
      xb_hash_copy(h, v.src);
      for (uint32_t i = 0; i < h->len; ++i)
        if (clone_item(&m, &h->entries[i].value)) xb_gc_write(h);
    } else {
      xb_array *a = v.copy.v.p;
      for (size_t i = 0; i < a->len; ++i)
        if (clone_item(&m, &a->items[i])) xb_gc_write_item(a, i);
    }
  }
  free(m.slots);
  free(m.order);
}
