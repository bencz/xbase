/* Array functions of the language. */
#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

/* Array( n1 [, n2 ...] ): nested arrays of NIL. */
static xb_array *make_dims(xb_ctx *ctx, uint32_t ndims, const xb_item *dims, xb_item *holder) {
  int64_t n = xb_num_to_i64(&dims[0]);
  if (n < 0) n = 0;
  xb_array *a = xb_array_alloc((uint64_t)n);
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
  xb_array_resize(argv[0].v.p, n < 0 ? 0 : (uint64_t)n);
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

/* ASort( aTarget, [nStart], [nCount], [bOrder] ): sorts the elements from
 * nStart (1 when missing or 0; past the end: nothing), nCount of them (the
 * rest when missing, 0 or too many), in place, and returns the array (NIL for
 * anything else). The sort is stable: a top-down merge sort of positions,
 * whose left half has n/2 of them and whose right element goes first only
 * when it is less, so elements that compare equal keep their order. bOrder
 * receives two elements and answers whether the first goes before the second
 * (a number: whether it is not 0; anything else: yes). Without it, values of
 * one kind compare like the operators (strings follow SET EXACT, integers
 * exactly, other numbers as doubles, dates by day, .F. before .T.), and other
 * pairs by kind: array, block, string, logical, date, number, NIL. A negative
 * nStart sorts nothing; a negative nCount means the rest. */
typedef struct {
  xb_ctx *ctx;
  const xb_item *array; /* the rooted argument: the array is read through it after every block call */
  const xb_item *block;
  xb_item *call;        /* rooted: the block's two arguments, then its result */
  int failed;           /* the block left an error or another request to unwind: the sort stops */
} sorter;

static int sort_kind(const xb_item *it) {
  switch ((xb_type)it->type) {
  case XB_T_ARRAY: return 1;
  case XB_T_BLOCK: return 2;
  case XB_T_STRING: return 3;
  case XB_T_LOGICAL: return 4;
  case XB_T_DATE: return 5;
  case XB_T_INT: case XB_T_DOUBLE: return 6;
  default: return 7;
  }
}

static int sort_less(sorter *s, size_t i, size_t j) {
  if (s->failed) return 0;
  const xb_array *a = s->array->v.p;
  if (s->block) {
    if (i >= a->len || j >= a->len) return 0; /* the block shrank the array */
    s->call[0] = a->items[i];
    s->call[1] = a->items[j];
    xb_block_call(s->ctx, &s->call[2], s->block, 2, s->call);
    if (s->ctx->pending && xb_pending(s->ctx)) {
      s->failed = 1;
      return 0;
    }
    const xb_item *r = &s->call[2];
    if (r->type == XB_T_LOGICAL) return r->v.l != 0;
    if (xb_is_number(r)) return xb_num_value(r) != 0.0;
    return 1;
  }
  const xb_item *x = &a->items[i], *y = &a->items[j];
  int c = xb_compare(s->ctx, x, y);
  return c == 2 ? sort_kind(x) < sort_kind(y) : c < 0;
}

static void merge_sort(sorter *s, size_t *pos, size_t *tmp, size_t n) {
  if (n < 2) return;
  size_t left = n / 2;
  merge_sort(s, pos, tmp, left);
  merge_sort(s, pos + left, tmp, n - left);
  size_t i = 0, j = left, k = 0;
  while (i < left && j < n) tmp[k++] = sort_less(s, pos[j], pos[i]) ? pos[j++] : pos[i++];
  while (i < left) tmp[k++] = pos[i++];
  while (j < n) tmp[k++] = pos[j++];
  memcpy(pos, tmp, n * sizeof *pos);
}

void xbf_ASORT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || argv[0].type != XB_T_ARRAY) return;
  const size_t len = ((const xb_array *)argv[0].v.p)->len;
  *ret = argv[0];
  uint64_t start = 1;
  if (argc > 1 && xb_is_number(&argv[1])) {
    int64_t v = xb_num_to_i64(&argv[1]);
    if (v < 0) return; /* a negative start is past any end */
    if (v > 0) start = (uint64_t)v;
  }
  if (start > len) return;
  uint64_t rest = len - start + 1, count = rest;
  if (argc > 2 && xb_is_number(&argv[2])) {
    int64_t v = xb_num_to_i64(&argv[2]);
    if (v >= 1 && (uint64_t)v < rest) count = (uint64_t)v;
  }
  if (count < 2 || count > XB_ARRAY_MAX_LEN) return; /* no array is longer: bounds the buffer below */
  size_t n = (size_t)count, first = (size_t)(start - 1);
  size_t *pos = malloc(2 * n * sizeof *pos);
  if (!pos) xb_gc_fatal("out of memory");
  for (size_t k = 0; k < n; ++k) pos[k] = first + k;
  xb_item call[3];
  for (int k = 0; k < 3; ++k) {
    xb_set_nil(&call[k]);
    xb_gc_root_push(ctx, &call[k]);
  }
  sorter s = {ctx, &argv[0], argc > 3 && argv[3].type == XB_T_BLOCK ? &argv[3] : NULL, call, 0};
  merge_sort(&s, pos, pos + n, n);
  xb_gc_root_pop(ctx, 3);
  if (!s.failed) {
    xb_array *a = argv[0].v.p;
    /* A block may have shrunk the array: the positions still in it go first. */
    size_t kept = 0;
    for (size_t k = 0; k < n; ++k)
      if (pos[k] < a->len) pos[kept++] = pos[k];
    xb_item *vals = malloc(kept * sizeof *vals + 1);
    if (!vals) xb_gc_fatal("out of memory");
    for (size_t k = 0; k < kept; ++k) vals[k] = a->items[pos[k]]; /* nothing allocates until they are back */
    for (size_t k = 0; k < kept; ++k) {
      a->items[first + k] = vals[k];
      xb_gc_write_item(a, first + k); /* a young element may move to another card */
    }
    free(vals);
  }
  free(pos);
}
