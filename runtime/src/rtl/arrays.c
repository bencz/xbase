/* Array functions of the language. */
#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

/* Array( n1 [, n2 ...] ): nested arrays of NIL. */
static xb_array *make_dims(xb_ctx *ctx, uint32_t ndims, const xb_item *dims, xb_item *holder) {
  int64_t n = xb_num_to_i64(&dims[0]);
  if (n < 0) n = 0;
  xb_array *a = xb_array_alloc((size_t)n);
  xb_set_array(holder, a); /* keeps `a` reachable while sub-arrays are allocated */
  if (ndims > 1) {
    xb_item sub;
    xb_set_nil(&sub);
    xb_gc_root_push(ctx, &sub);
    for (int64_t i = 0; i < n; ++i) {
      make_dims(ctx, ndims - 1, dims + 1, &sub);
      a->items[i] = sub;
      xb_gc_write_item(a, (size_t)i); /* `a` may have aged while the sub-array was allocated */
    }
    xb_gc_root_pop(ctx, 1);
  }
  return a;
}

void xbf_ARRAY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc == 0) return;
  for (uint32_t i = 0; i < argc; ++i) {
    if (!xb_is_number(&argv[i])) return; /* NIL, no error */
    if (xb_num_value(&argv[i]) < 0) {
      xb_rt_error(ctx, "BASE", 1131, "Bound error", "array dimension");
      return;
    }
  }
  make_dims(ctx, argc, argv, ret);
}

void xbf_AADD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || argv[0].type != XB_T_ARRAY) {
    xb_rt_error(ctx, "BASE", 1123, "Argument error", "AADD");
    return;
  }
  xb_array *a = argv[0].v.p;
  xb_array_resize(a, a->len + 1);
  if (argc > 1) {
    a->items[a->len - 1] = argv[1];
    xb_gc_write_item(a, a->len - 1);
  }
  *ret = a->items[a->len - 1];
}

void xbf_ASIZE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 2 || argv[0].type != XB_T_ARRAY || !xb_is_number(&argv[1])) {
    xb_rt_error(ctx, "BASE", 2023, "Argument error", "ASIZE");
    return;
  }
  int64_t n = xb_num_to_i64(&argv[1]);
  xb_array_resize(argv[0].v.p, n < 0 ? 0 : (size_t)n);
  *ret = argv[0];
}

void xbf_ATAIL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (argc < 1 || argv[0].type != XB_T_ARRAY) return;
  const xb_array *a = argv[0].v.p;
  if (a->len) *ret = a->items[a->len - 1];
}

/* AClone(): nested arrays are copied too, keeping the structure: an array
 * reached twice is copied once, so shared and cyclic arrays stay so. Works
 * with an explicit list, without recursion. */
typedef struct clone_entry {
  const xb_array *src;
  xb_array *copy;
} clone_entry;

typedef struct clone_map {       /* open addressing, power-of-two size */
  clone_entry *slots;
  size_t mask, count;
  const xb_array **order;        /* copies to visit, in creation order */
  size_t orderLen, orderCap;
} clone_map;

static size_t slot_of(const clone_map *m, const xb_array *src) {
  size_t h = ((uintptr_t)src >> 4) * (size_t)0x9E3779B97F4A7C15ull;
  for (size_t i = h & m->mask;; i = (i + 1) & m->mask)
    if (!m->slots[i].src || m->slots[i].src == src) return i;
}

static void map_add(clone_map *m, const xb_array *src, xb_array *copy) {
  if ((m->count + 1) * 2 > m->mask + 1) {
    clone_map grown = {calloc((m->mask + 1) * 2, sizeof(clone_entry)), (m->mask + 1) * 2 - 1, 0, m->order,
                       m->orderLen, m->orderCap};
    if (!grown.slots) abort();
    for (size_t i = 0; i <= m->mask; ++i)
      if (m->slots[i].src) grown.slots[slot_of(&grown, m->slots[i].src)] = m->slots[i];
    grown.count = m->count;
    free(m->slots);
    *m = grown;
  }
  m->slots[slot_of(m, src)] = (clone_entry){src, copy};
  if (m->orderLen == m->orderCap) {
    m->orderCap = m->orderCap ? m->orderCap * 2 : 16;
    m->order = realloc(m->order, m->orderCap * sizeof *m->order);
    if (!m->order) abort();
  }
  m->order[m->orderLen++] = src;
  ++m->count;
}

void xbf_ACLONE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (argc < 1 || argv[0].type != XB_T_ARRAY) return;
  clone_map m = {calloc(16, sizeof(clone_entry)), 15, 0, NULL, 0, 0};
  if (!m.slots) abort();
  const xb_array *root = argv[0].v.p;
  xb_array *top = xb_array_alloc(root->len);
  xb_set_array(ret, top); /* every copy below hangs from `ret` once allocated */
  memcpy(top->items, root->items, root->len * sizeof(xb_item));
  map_add(&m, root, top);
  for (size_t next = 0; next < m.orderLen; ++next) {
    xb_array *copy = m.slots[slot_of(&m, m.order[next])].copy;
    for (size_t i = 0; i < copy->len; ++i) {
      if (copy->items[i].type != XB_T_ARRAY) continue;
      const xb_array *src = copy->items[i].v.p;
      clone_entry *e = &m.slots[slot_of(&m, src)];
      xb_array *dst = e->src ? e->copy : NULL;
      if (!dst) {
        dst = xb_array_alloc(src->len); /* `src` is still referenced by copy->items[i] */
        memcpy(dst->items, src->items, src->len * sizeof(xb_item));
        map_add(&m, src, dst);
      }
      xb_set_array(&copy->items[i], dst);
      xb_gc_write_item(copy, i);
    }
  }
  free(m.slots);
  free(m.order);
}
