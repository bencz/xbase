/* Array functions of the language. */
#include "rtl/collections.h"

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
      xb_error_in(ctx, "ARRAY", NULL, EG_BOUND, 1131, "array dimension", 0, argc, argv);
      return;
    }
  }
  make_dims(ctx, argc, argv, ret);
}

void xbf_AADD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || argv[0].type != XB_T_ARRAY) {
    xb_fn_arg_error(ctx, ret, 1123, "AADD", argc, argv);
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
    xb_error_in(ctx, "ASIZE", NULL, EG_ARG, 2023, "ASIZE", 0, argc, argv);
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

/* AClone( aSource ): a copy, the arrays and hashes inside it copied too
 * (xb_clone_deep). */
void xbf_ACLONE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (argc < 1 || argv[0].type != XB_T_ARRAY) return;
  xb_clone_deep(ret, &argv[0]);
}

/* ASort( aTarget, [nStart], [nCount], [bOrder] ): sorts the elements from
 * nStart (1 when missing or 0; past the end: nothing), nCount of them (the
 * rest when missing, 0 or too many), in place, and returns the array (NIL for
 * anything else). The sort is stable (xb_merge_sort of positions). bOrder
 * receives two elements and answers whether the first goes before the second
 * (xb_order_answer); without it, xb_value_less orders them. A negative nStart
 * sorts nothing; a negative nCount means the rest. */
typedef struct {
  xb_ctx *ctx;
  const xb_item *array; /* the rooted argument: the array is read through it after every block call */
  const xb_item *block;
  xb_item *call;        /* rooted: the block's two arguments, then its result */
  int failed;           /* the block left a request to unwind (a BREAK, a QUIT): the sort stops */
} sorter;

static int sort_less(void *arg, size_t i, size_t j) {
  sorter *s = arg;
  if (s->failed) return 0;
  const xb_array *a = s->array->v.p;
  if (s->block) {
    if (i >= a->len || j >= a->len) return 0; /* the block shrank the array */
    s->call[0] = a->items[i];
    s->call[1] = a->items[j];
    if (!xb_calls_run(s->ctx, s->block, s->call, 2)) {
      s->failed = 1;
      return 0;
    }
    return xb_order_answer(&s->call[2]);
  }
  return xb_value_less(s->ctx, &a->items[i], &a->items[j]);
}

/* The order xb_value_less gives two integers, or two doubles (NaN before
 * nothing), for a range whose elements are all of that type. */
static int int_less(const xb_item *items, size_t i, size_t j) { return items[i].v.i < items[j].v.i; }
static int double_less(const xb_item *items, size_t i, size_t j) {
  return xb_order_double(items[i].v.d, items[j].v.d) == -1;
}
XB_DEFINE_MERGE_SORT(sort_ints, const xb_item *, int_less)
XB_DEFINE_MERGE_SORT(sort_doubles, const xb_item *, double_less)

/* The type every element of items[first .. first+n-1] has, else XB_T_NIL. */
static uint32_t common_type(const xb_item *items, size_t first, size_t n) {
  uint32_t t = items[first].type;
  for (size_t k = 1; k < n; ++k)
    if (items[first + k].type != t) return XB_T_NIL;
  return t;
}

/* Calls `block` with an element and its position (1-based), as AScan and AEval
 * do; `call` holds three rooted items: the two arguments, then the result.
 * Returns 0 when the block left a request to unwind. */
static int call_with_element(xb_ctx *ctx, const xb_item *block, xb_item *call, const xb_item *elem, size_t i) {
  call[0] = *elem;
  xb_set_int(&call[1], (int64_t)i + 1);
  return xb_calls_run(ctx, block, call, 2);
}

static const xb_proc ascan_proc = XB_RUNTIME_PROC("ASCAN", 0), asort_proc = XB_RUNTIME_PROC("ASORT", 0);

/* AScan( aTarget, xSearch, [nStart], [nCount] ): the position of the first
 * element in the range that is equal to xSearch (values of one comparable
 * kind, with SET EXACT for strings, or two NILs; arrays, blocks and objects
 * never match), or for which the block xSearch( element, position ) returns
 * .T.; 0 when none is (or aTarget is not an array). */
void xbf_ASCAN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_int(ret, 0);
  if (argc < 2 || argv[0].type != XB_T_ARRAY) return; /* without xSearch (even NIL) nothing matches */
  uint64_t start = xb_range_start(argc, argv, 2, ((const xb_array *)argv[0].v.p)->len);
  if (!start) return;
  uint64_t count = xb_range_count(argc, argv, 3, ((const xb_array *)argv[0].v.p)->len - start + 1);
  const xb_item *search = &argv[1];
  if (search->type == XB_T_BLOCK) {
    xb_item call[3];
    xb_frame frame;
    xb_calls_begin(ctx, call, 3, &frame, &ascan_proc);
    for (size_t i = (size_t)(start - 1), end = i + (size_t)count; i < end; ++i) {
      const xb_array *a = argv[0].v.p; /* the block may change the array */
      if (i >= a->len) break;
      if (!call_with_element(ctx, search, call, &a->items[i], i)) break;
      if (call[2].type == XB_T_LOGICAL && call[2].v.l) {
        xb_set_int(ret, (int64_t)i + 1);
        break;
      }
    }
    xb_calls_end(ctx, &frame, 3);
    return;
  }
  const xb_array *a = argv[0].v.p;
  for (size_t i = (size_t)(start - 1), end = i + (size_t)count; i < end; ++i) {
    if (xb_value_matches(ctx, &a->items[i], search)) {
      xb_set_int(ret, (int64_t)i + 1);
      return;
    }
  }
}

/* __RangeStart( nLen, [nStart] ): where the range of a collection of nLen
 * items starts, as the collection functions take nStart (xb_range_start): 1
 * by default, 0 when the range is empty. For the runtime's xBase code. */
void xbf___RANGESTART(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  int64_t len = argc && xb_is_number(&argv[0]) ? xb_num_to_i64(&argv[0]) : 0;
  xb_set_int(ret, (int64_t)xb_range_start(argc, argv, 1, len > 0 ? (size_t)len : 0));
}

/* __RangeCount( nRest, [nCount] ): how many items of the nRest from the
 * start the range takes (xb_range_count): all of them by default. */
void xbf___RANGECOUNT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  int64_t rest = argc && xb_is_number(&argv[0]) ? xb_num_to_i64(&argv[0]) : 0;
  xb_set_int(ret, (int64_t)xb_range_count(argc, argv, 1, rest > 0 ? (uint64_t)rest : 0));
}

/* The 1-based position argument of AIns and ADel (0 or missing is 1), as a
 * 0-based index, or -1 when it is outside the array. */
static int64_t element_index(const xb_array *a, uint32_t argc, const xb_item *argv) {
  int64_t n = argc > 1 && xb_is_number(&argv[1]) ? xb_num_to_i64(&argv[1]) : 0;
  return xb_array_index(a, n == 0 ? 1 : n);
}

/* AIns( aTarget, nPosition ): the elements from nPosition (0 is 1) move one place up
 * (the last one is lost) and nPosition becomes NIL; the array keeps its
 * length. Returns the array (NIL for anything else). */
void xbf_AINS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (argc < 1 || argv[0].type != XB_T_ARRAY) return;
  *ret = argv[0];
  xb_array *a = argv[0].v.p;
  int64_t at = element_index(a, argc, argv);
  if (at < 0) return;
  for (size_t i = a->len - 1; i > (size_t)at; --i) {
    a->items[i] = a->items[i - 1];
    xb_gc_write_item(a, i);
  }
  xb_set_nil(&a->items[at]);
}

/* ADel( aTarget, nPosition ): the elements after nPosition (0 is 1) move one
 * place down and the last becomes NIL; the array keeps its length. Returns the
 * array (NIL for anything else). */
void xbf_ADEL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (argc < 1 || argv[0].type != XB_T_ARRAY) return;
  *ret = argv[0];
  xb_array *a = argv[0].v.p;
  int64_t at = element_index(a, argc, argv);
  if (at < 0) return;
  for (size_t i = (size_t)at; i + 1 < a->len; ++i) {
    a->items[i] = a->items[i + 1];
    xb_gc_write_item(a, i);
  }
  xb_set_nil(&a->items[a->len - 1]);
}

/* ACopy( aSource, aTarget, [nStart], [nCount], [nTargetPos] ): copies the
 * range of aSource into aTarget from nTargetPos (missing or 0: 1; past the
 * end or negative: the last position), as many elements as fit. Returns
 * aTarget (NIL unless both are arrays). */
void xbf_ACOPY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  if (argc < 2 || argv[0].type != XB_T_ARRAY || argv[1].type != XB_T_ARRAY) return;
  *ret = argv[1];
  const xb_array *src = argv[0].v.p;
  xb_array *dst = argv[1].v.p;
  uint64_t start = xb_range_start(argc, argv, 2, src->len);
  if (!start || dst->len == 0) return;
  uint64_t count = xb_range_count(argc, argv, 3, src->len - start + 1);
  uint64_t target = 1;
  if (argc > 4 && xb_is_number(&argv[4])) {
    int64_t v = xb_num_to_i64(&argv[4]);
    if (v < 0 || (uint64_t)v > dst->len) target = dst->len;
    else if (v > 0) target = (uint64_t)v;
  }
  if (count > dst->len - target + 1) count = dst->len - target + 1;
  for (uint64_t k = 0; k < count; ++k) {
    size_t to = (size_t)(target - 1 + k);
    dst->items[to] = src->items[start - 1 + k];
    xb_gc_write_item(dst, to);
  }
}

/* AFill( aTarget, xValue, [nStart], [nCount] ): the range from nStart, up to
 * nCount elements (the rest of the array without it; none when it is 0 or
 * less) takes the value; without a value nothing changes. */
void xbf_AFILL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || argv[0].type != XB_T_ARRAY) {
    xb_error_in(ctx, "AFILL", NULL, EG_ARG, 6004, "AFILL", 0, argc, argv);
    return;
  }
  *ret = argv[0];
  xb_array *a = argv[0].v.p;
  uint64_t start = argc > 1 ? xb_range_start(argc, argv, 2, a->len) : 0;
  if (!start) return;
  uint64_t count = a->len - start + 1;
  if (argc > 3 && xb_is_number(&argv[3])) {
    int64_t v = xb_num_to_i64(&argv[3]);
    if (v <= 0) return;
    if ((uint64_t)v < count) count = (uint64_t)v;
  }
  for (size_t k = (size_t)(start - 1), end = k + (size_t)count; k < end; ++k) {
    a->items[k] = argv[1];
    xb_gc_write_item(a, k);
  }
}

void xbf_ASORT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc < 1 || argv[0].type != XB_T_ARRAY) return;
  const size_t len = ((const xb_array *)argv[0].v.p)->len;
  *ret = argv[0];
  uint64_t start = xb_range_start(argc, argv, 1, len);
  if (!start) return;
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
  xb_frame frame;
  xb_calls_begin(ctx, call, 3, &frame, &asort_proc);
  sorter s = {ctx, &argv[0], argc > 3 && argv[3].type == XB_T_BLOCK ? &argv[3] : NULL, call, 0};
  const xb_item *items = ((const xb_array *)argv[0].v.p)->items;
  uint32_t same = s.block ? XB_T_NIL : common_type(items, first, n); /* without a block nothing runs: no change */
  if (same == XB_T_INT) sort_ints(pos, pos + n, n, items);
  else if (same == XB_T_DOUBLE) sort_doubles(pos, pos + n, n, items);
  else xb_merge_sort(pos, pos + n, n, sort_less, &s);
  xb_calls_end(ctx, &frame, 3);
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
