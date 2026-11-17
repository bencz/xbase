/* The hash functions (types/hash.c holds the hash itself). A first argument
 * that is not a hash, or a key that cannot be one, is BASE/1123 (not
 * substitutable); positions are 1-based and count live entries, and one
 * outside 1 .. Len() is BASE/1187. Blocks receive the key, the value and the
 * position, and see the hash as it is at each call. */
#include "rtl/collections.h"

#include "core/shared.h"

#include <stdlib.h>
#include <string.h>

/* The hash argument `k` of `fn`, or NULL after raising the error. */
static xb_hash *hash_arg(xb_ctx *ctx, const char *fn, uint32_t argc, xb_item *argv, uint32_t k) {
  if (argc > k && argv[k].type == XB_T_HASH) return argv[k].v.p;
  xb_error_in(ctx, fn, NULL, EG_ARG, 1123, fn, 0, argc, argv);
  return NULL;
}

/* The first argument as a hash and the second as a key, or 0 after the error. */
static int hash_and_key(xb_ctx *ctx, const char *fn, uint32_t argc, xb_item *argv, xb_hash **h) {
  *h = hash_arg(ctx, fn, argc, argv, 0);
  if (!*h) return 0;
  if (argc > 1 && xb_hash_key_ok(&argv[1])) return 1;
  xb_error_in(ctx, fn, NULL, EG_ARG, 1123, fn, 0, argc, argv);
  return 0;
}

/* The position argument 1 of `fn` (0 for none). */
static int64_t pos_arg(uint32_t argc, const xb_item *argv) {
  return argc > 1 && xb_is_number(&argv[1]) ? xb_num_to_i64(&argv[1]) : 0;
}

/* The bound error of a position outside the hash. */
static void pos_error(xb_ctx *ctx, const char *fn, uint32_t argc, xb_item *argv) {
  xb_error_in(ctx, fn, NULL, EG_BOUND, 1187, fn, 0, argc, argv);
}

/* Hash( [xKey, xValue, ...] ): a new hash of the pairs. A key without a value
 * is BASE/1131; a value that cannot be a key is the error of a literal with
 * one (xb_hash_new). */
void xbf_HASH(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (argc % 2) {
    xb_error_in(ctx, "HASH", NULL, EG_BOUND, 1131, "array dimension", 0, argc, argv);
    return;
  }
  xb_hash *h = xb_hash_alloc();
  xb_set_hash(ret, h);
  xb_hash_reserve(h, argc / 2);
  for (uint32_t k = 0; k < argc; k += 2) {
    if (!xb_hash_key_ok(&argv[k])) {
      xb_item args[3] = {*ret, argv[k], argv[k + 1]}; /* `ret` keeps the hash rooted while the error is made */
      xb_error_in(ctx, "HASH", NULL, EG_BOUND, 1133, "array assign", 0, 3, args);
      xb_set_nil(ret);
      return;
    }
    xb_hash_set(h, &argv[k], &argv[k + 1]);
  }
}

/* HGet( hHash, xKey ): the value, as hHash[xKey] (a missing key is a bound
 * error). */
void xbf_HGET(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (!hash_and_key(ctx, "HGET", argc, argv, &h)) return;
  if (!xb_hash_get(h, &argv[1], ret)) xb_array_bound_error(ctx, &argv[0], &argv[1]);
}

/* HGetDef( hHash, xKey, [xDefault] ): the value, or xDefault when the key is
 * missing. */
void xbf_HGETDEF(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (!hash_and_key(ctx, "HGETDEF", argc, argv, &h)) return;
  if (!xb_hash_get(h, &argv[1], ret) && argc > 2) *ret = argv[2];
}

/* HSet( hHash, xKey, xValue ): sets the key (adding it last when new);
 * returns hHash. */
void xbf_HSET(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (!hash_and_key(ctx, "HSET", argc, argv, &h)) return;
  xb_item nil;
  xb_set_nil(&nil);
  xb_hash_put(ctx, h, &argv[1], argc > 2 ? &argv[2] : &nil);
  *ret = argv[0];
}

/* HHasKey( hHash, xKey ): whether the key is there. */
void xbf_HHASKEY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (hash_and_key(ctx, "HHASKEY", argc, argv, &h)) xb_set_logical(ret, xb_hash_get(h, &argv[1], NULL));
}

/* HDel( hHash, xKey ): removes the key if it is there; returns hHash. */
void xbf_HDEL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (!hash_and_key(ctx, "HDEL", argc, argv, &h)) return;
  xb_hash_remove(h, &argv[1]);
  *ret = argv[0];
}

/* HDelAt( hHash, nPos ): removes the entry at nPos; returns hHash. */
void xbf_HDELAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HDELAT", argc, argv, 0);
  if (!h) return;
  if (!xb_hash_remove_at(h, pos_arg(argc, argv))) pos_error(ctx, "HDELAT", argc, argv);
  else *ret = argv[0];
}

/* HPos( hHash, xKey ): the key's position, 0 when it is missing. */
void xbf_HPOS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (hash_and_key(ctx, "HPOS", argc, argv, &h)) xb_set_int(ret, xb_hash_position(h, &argv[1]));
}

/* HKeyAt( hHash, nPos ): the key at nPos. */
void xbf_HKEYAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HKEYAT", argc, argv, 0);
  if (h && !xb_hash_entry(h, pos_arg(argc, argv), ret, NULL)) pos_error(ctx, "HKEYAT", argc, argv);
}

/* HValueAt( hHash, nPos, [xNew] ): the value at nPos, which xNew replaces. */
void xbf_HVALUEAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HVALUEAT", argc, argv, 0);
  if (h && !xb_hash_value_at(h, pos_arg(argc, argv), argc > 2 ? &argv[2] : NULL, ret))
    pos_error(ctx, "HVALUEAT", argc, argv);
}

/* HPairAt( hHash, nPos ): { key, value } at nPos. */
void xbf_HPAIRAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HPAIRAT", argc, argv, 0);
  if (!h) return;
  xb_item pair[2]; /* rooted through the allocation: another thread may remove the entry */
  xb_set_nil(&pair[0]);
  xb_set_nil(&pair[1]);
  xb_gc_root_push(ctx, &pair[0]);
  xb_gc_root_push(ctx, &pair[1]);
  if (xb_hash_entry(h, pos_arg(argc, argv), &pair[0], &pair[1])) {
    xb_array *a = xb_array_alloc(2);
    a->items[0] = pair[0];
    a->items[1] = pair[1];
    xb_set_array(ret, a);
  } else {
    pos_error(ctx, "HPAIRAT", argc, argv);
  }
  xb_gc_root_pop(ctx, 2);
}

/* The keys (0) or the values (1) as an array. */
static void hash_column(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, const char *fn, int values) {
  xb_hash *h = hash_arg(ctx, fn, argc, argv, 0);
  if (!h) return;
  const xb_item *from = &argv[0]; /* a local hash in place; a shared one through a copy, rooted in `ret` */
  if (xb_is_shared(h)) {
    xb_hash_snapshot(ret, h);
    from = ret;
  }
  xb_array *a = xb_array_alloc(((const xb_hash *)from->v.p)->len);
  xb_hash *src = from->v.p; /* where it is after the allocation */
  xb_hash_compact(src);
  for (uint32_t k = 0; k < src->len; ++k) a->items[k] = values ? src->entries[k].value : src->entries[k].key;
  xb_set_array(ret, a);
}

/* HKeys( hHash ), HValues( hHash ): the keys, the values, in order. */
void xbf_HKEYS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  hash_column(ctx, ret, argc, argv, "HKEYS", 0);
}
void xbf_HVALUES(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  hash_column(ctx, ret, argc, argv, "HVALUES", 1);
}

/* HClone( hHash ): a copy, the arrays and hashes inside it copied too
 * (xb_clone_deep), with the same letter case rule. */
void xbf_HCLONE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (hash_arg(ctx, "HCLONE", argc, argv, 0)) xb_clone_deep(ctx, ret, &argv[0]);
}

/* HCopy( hSource, hDest, [nStart], [nCount] ): adds the entries of hSource
 * in the range to hDest; returns hDest. */
void xbf_HCOPY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *src = hash_arg(ctx, "HCOPY", argc, argv, 0);
  xb_hash *dest = src ? hash_arg(ctx, "HCOPY", argc, argv, 1) : NULL;
  if (!dest) return;
  *ret = argv[1];
  xb_item copy; /* a shared source's entries as one locked read saw them; a local one is read in place */
  xb_set_nil(&copy);
  xb_gc_root_push(ctx, &copy);
  const xb_item *from = &argv[0];
  if (xb_is_shared(src)) {
    xb_hash_snapshot(&copy, src);
    from = &copy;
  }
  xb_hash_compact(from->v.p); /* positions are entries */
  uint32_t len = ((const xb_hash *)from->v.p)->len;
  uint64_t start = xb_range_start(argc, argv, 2, len);
  if (start) {
    uint64_t count = xb_range_count(argc, argv, 3, len - start + 1);
    if (!xb_hash_reserve_try(ctx, argv[1].v.p, (uint64_t)xb_hash_length(argv[1].v.p) + count)) {
      xb_gc_root_pop(ctx, 1);
      xb_error_memory(ctx, ret, "HCOPY", argc, argv);
      return;
    }
    for (uint64_t k = start - 1; k < start - 1 + count; ++k) {
      xb_hash *s = from->v.p; /* read again after each put: a collection may move it */
      if (k >= s->len) break; /* a local source that is the destination grows only: never */
      xb_item pair[2] = {s->entries[k].key, s->entries[k].value}; /* `from` keeps them alive */
      xb_hash_put(ctx, argv[1].v.p, &pair[0], &pair[1]);
    }
  }
  xb_gc_root_pop(ctx, 1);
}


typedef struct {
  xb_ctx *ctx;
  xb_hash *h;
  const xb_item *block;
  xb_item *call;    /* rooted: the two keys, then the block's result */
  uint32_t len;     /* the entries the sort started with */
  int failed;       /* the block unwound or changed the hash's size: the sort stops */
} hash_sorter;

static int key_less(void *arg, size_t i, size_t j) {
  hash_sorter *s = arg;
  if (s->failed) return 0;
  if (xb_is_shared(s->h)) { /* the block shared it: no more reads here; it is sorted again as a shared one */
    s->failed = 1;
    return 0;
  }
  if (s->h->len != s->len) {
    s->failed = 1;
    return 0;
  }
  const xb_item *x = &xb_hash_at(s->h, (int64_t)i + 1)->key, *y = &xb_hash_at(s->h, (int64_t)j + 1)->key;
  if (!s->block) return xb_hash_key_order(s->h, x, y) < 0;
  s->call[0] = *x;
  s->call[1] = *y;
  if (!xb_calls_run(s->ctx, s->block, s->call, 2)) {
    s->failed = 1;
    return 0;
  }
  return xb_order_answer(&s->call[2]);
}

static const xb_proc hsort_proc = XB_RUNTIME_PROC("HSORT", 0);

void xbf_HSORT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

/* HSort of a shared hash: a block may run any code, so a copy (the entries as one locked read saw them) is sorted,
 * and its entries go back under the lock when the hash still has as many (else it stays as another thread left it). */
static void sort_shared(xb_ctx *ctx, uint32_t argc, xb_item *argv) {
  xb_item args[2], sorted;
  xb_set_nil(&args[0]);
  xb_set_nil(&args[1]);
  xb_set_nil(&sorted);
  if (argc > 1) args[1] = argv[1];
  xb_gc_root_push(ctx, &args[0]);
  xb_gc_root_push(ctx, &args[1]);
  xb_gc_root_push(ctx, &sorted);
  xb_hash_snapshot(&args[0], argv[0].v.p);
  xbf_HSORT(ctx, &sorted, argc > 1 ? 2 : 1, args);
  if (!(xb_pending_get(ctx) & XB_PENDING_UNWIND)) xb_hash_take_order(argv[0].v.p, args[0].v.p);
  xb_gc_root_pop(ctx, 3);
}

/* HSort( hHash, [bOrder] ): orders the entries by key, stably, by bOrder(
 * key1, key2 ) (whether the first goes first) or in the hash's key order
 * (xb_hash_key_order); returns hHash. */
void xbf_HSORT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HSORT", argc, argv, 0);
  if (!h) return;
  *ret = argv[0];
  if (xb_is_shared(h)) {
    sort_shared(ctx, argc, argv);
    return;
  }
  size_t n = h->len;
  if (n < 2) return;
  xb_hash_compact(h); /* the comparisons find positions directly */
  size_t *pos = malloc(2 * n * sizeof *pos);
  if (!pos) xb_gc_fatal("out of memory");
  for (size_t k = 0; k < n; ++k) pos[k] = k;
  xb_item call[3];
  xb_frame frame;
  xb_calls_begin(ctx, call, 3, &frame, &hsort_proc);
  hash_sorter s = {ctx, h, argc > 1 && argv[1].type == XB_T_BLOCK ? &argv[1] : NULL, call, h->len, 0};
  xb_merge_sort(pos, pos + n, n, key_less, &s);
  xb_calls_end(ctx, &frame, 3);
  if (xb_is_shared(argv[0].v.p) && !(xb_pending_get(ctx) & XB_PENDING_UNWIND)) { /* the block shared it */
    free(pos);
    sort_shared(ctx, argc, argv);
    return;
  }
  if (!s.failed && h->len == n) {
    xb_hash_compact(h); /* positions are entries */
    xb_hentry *sorted = malloc(n * sizeof *sorted);
    if (!sorted) xb_gc_fatal("out of memory");
    for (size_t k = 0; k < n; ++k) sorted[k] = h->entries[pos[k]]; /* nothing allocates until they are back */
    memcpy(h->entries, sorted, n * sizeof *sorted);
    free(sorted);
    xb_hash_reindex(h);
  }
  free(pos);
}

/* HClear( hHash ): removes every entry; returns hHash. */
void xbf_HCLEAR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HCLEAR", argc, argv, 0);
  if (!h) return;
  xb_hash_empty(h);
  *ret = argv[0];
}

/* HFill( hHash, xValue ): gives every key the value; returns hHash. */
void xbf_HFILL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HFILL", argc, argv, 0);
  if (!h) return;
  xb_item nil;
  xb_set_nil(&nil);
  xb_hash_fill(h, argc > 1 ? &argv[1] : &nil);
  *ret = argv[0];
}

/* HAllocate( hHash, nCount ): room for nCount entries; returns hHash. */
void xbf_HALLOCATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HALLOCATE", argc, argv, 0);
  if (!h) return;
  int64_t n = argc > 1 && xb_is_number(&argv[1]) ? xb_num_to_i64(&argv[1]) : 0;
  if (n > 0 && !xb_hash_reserve_try(ctx, h, (uint64_t)n)) { /* BASE/9032 Memory low: past what memory holds */
    xb_error_memory(ctx, ret, "HALLOCATE", argc, argv);
    return;
  }
  *ret = argv[0];
}

/* HCaseMatch( hHash, [lMatch] ): whether letter case matters in its string
 * keys (.T. for a new hash); lMatch changes it. Keys that then become equal merge
 * (xb_hash_set_flags). */
void xbf_HCASEMATCH(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HCASEMATCH", argc, argv, 0);
  if (!h) return;
  int change = argc > 1 && argv[1].type == XB_T_LOGICAL;
  xb_set_logical(ret, xb_hash_case_match(h, change, change && argv[1].v.l));
}
