/* The hash functions (types/hash.c holds the hash itself). A first argument
 * that is not a hash, or a key that cannot be one, is BASE/1123 (not
 * substitutable); positions are 1-based and count live entries, and one
 * outside 1 .. Len() is BASE/1187. Blocks receive the key, the value and the
 * position, and see the hash as it is at each call. */
#include "rtl/collections.h"

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

/* The entry at position argument 1 of `fn`, or NULL after the bound error. */
static xb_hentry *entry_arg(xb_ctx *ctx, const char *fn, xb_hash *h, uint32_t argc, xb_item *argv) {
  int64_t pos = argc > 1 && xb_is_number(&argv[1]) ? xb_num_to_i64(&argv[1]) : 0;
  xb_hentry *e = xb_hash_at(h, pos);
  if (!e) xb_error_in(ctx, fn, NULL, EG_BOUND, 1187, fn, 0, argc, argv);
  return e;
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
  int64_t e = xb_hash_find(h, &argv[1]);
  if (e < 0) xb_array_bound_error(ctx, &argv[0], &argv[1]);
  else *ret = h->entries[e].value;
}

/* HGetDef( hHash, xKey, [xDefault] ): the value, or xDefault when the key is
 * missing. */
void xbf_HGETDEF(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (!hash_and_key(ctx, "HGETDEF", argc, argv, &h)) return;
  int64_t e = xb_hash_find(h, &argv[1]);
  if (e >= 0) *ret = h->entries[e].value;
  else if (argc > 2) *ret = argv[2];
}

/* HSet( hHash, xKey, xValue ): sets the key (adding it last when new);
 * returns hHash. */
void xbf_HSET(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (!hash_and_key(ctx, "HSET", argc, argv, &h)) return;
  xb_item nil;
  xb_set_nil(&nil);
  xb_hash_set(h, &argv[1], argc > 2 ? &argv[2] : &nil);
  *ret = argv[0];
}

/* HHasKey( hHash, xKey ): whether the key is there. */
void xbf_HHASKEY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (hash_and_key(ctx, "HHASKEY", argc, argv, &h)) xb_set_logical(ret, xb_hash_find(h, &argv[1]) >= 0);
}

/* HDel( hHash, xKey ): removes the key if it is there; returns hHash. */
void xbf_HDEL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (!hash_and_key(ctx, "HDEL", argc, argv, &h)) return;
  xb_hash_del(h, &argv[1]);
  *ret = argv[0];
}

/* HDelAt( hHash, nPos ): removes the entry at nPos; returns hHash. */
void xbf_HDELAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HDELAT", argc, argv, 0);
  if (!h || !entry_arg(ctx, "HDELAT", h, argc, argv)) return;
  xb_hash_del_at(h, xb_num_to_i64(&argv[1]));
  *ret = argv[0];
}

/* HPos( hHash, xKey ): the key's position, 0 when it is missing. */
void xbf_HPOS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h;
  if (hash_and_key(ctx, "HPOS", argc, argv, &h)) xb_set_int(ret, xb_hash_pos(h, &argv[1]));
}

/* HKeyAt( hHash, nPos ): the key at nPos. */
void xbf_HKEYAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HKEYAT", argc, argv, 0);
  const xb_hentry *e = h ? entry_arg(ctx, "HKEYAT", h, argc, argv) : NULL;
  if (e) *ret = e->key;
}

/* HValueAt( hHash, nPos, [xNew] ): the value at nPos, which xNew replaces. */
void xbf_HVALUEAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HVALUEAT", argc, argv, 0);
  xb_hentry *e = h ? entry_arg(ctx, "HVALUEAT", h, argc, argv) : NULL;
  if (!e) return;
  if (argc > 2) {
    e->value = argv[2];
    xb_gc_write(h);
  }
  *ret = e->value;
}

/* HPairAt( hHash, nPos ): { key, value } at nPos. */
void xbf_HPAIRAT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HPAIRAT", argc, argv, 0);
  const xb_hentry *e = h ? entry_arg(ctx, "HPAIRAT", h, argc, argv) : NULL;
  if (!e) return;
  xb_array *a = xb_array_alloc(2); /* a collection moves nothing: `e` stays valid */
  a->items[0] = e->key;
  a->items[1] = e->value;
  xb_set_array(ret, a);
}

/* The keys (0) or the values (1) as an array. */
static void hash_column(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv, const char *fn, int values) {
  xb_hash *h = hash_arg(ctx, fn, argc, argv, 0);
  if (!h) return;
  xb_array *a = xb_array_alloc(h->len);
  xb_hash_compact(h);
  for (uint32_t k = 0; k < h->len; ++k) a->items[k] = values ? h->entries[k].value : h->entries[k].key;
  xb_set_array(ret, a);
}

/* HKeys( hHash ), HValues( hHash ): the keys, the values, in order. */
void xbf_HKEYS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  hash_column(ctx, ret, argc, argv, "HKEYS", 0);
}
void xbf_HVALUES(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  hash_column(ctx, ret, argc, argv, "HVALUES", 1);
}

static const xb_proc hscan_proc = {"HSCAN", 0, 0}, heval_proc = {"HEVAL", 0, 0}, hmerge_proc = {"HMERGE", 0, 0},
                     hsort_proc = {"HSORT", 0, 0};

/* Calls `block` with the entry at `pos` (key, value, position) in call[0..2],
 * its result in call[3]; 0 when the entry is gone or the block unwinds. */
static int call_with_entry(xb_ctx *ctx, const xb_item *block, xb_item *call, xb_hash *h, int64_t pos) {
  const xb_hentry *e = xb_hash_at(h, pos);
  if (!e) return 0;
  call[0] = e->key;
  call[1] = e->value;
  xb_set_int(&call[2], pos);
  return xb_calls_run(ctx, block, call, 3);
}

/* HScan( hHash, xValue | bBlock, [nStart], [nCount] ): the position of the
 * first entry in the range whose value matches xValue (as AScan matches), or
 * for which bBlock( key, value, position ) returns .T.; 0 when none does. */
void xbf_HSCAN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HSCAN", argc, argv, 0);
  if (!h) return;
  xb_set_int(ret, 0);
  uint64_t start = argc > 1 ? xb_range_start(argc, argv, 2, h->len) : 0;
  if (!start) return;
  uint64_t count = xb_range_count(argc, argv, 3, h->len - start + 1);
  if (argv[1].type == XB_T_BLOCK) {
    xb_item call[4];
    xb_frame frame;
    xb_calls_begin(ctx, call, 4, &frame, &hscan_proc);
    for (uint64_t pos = start; pos < start + count; ++pos) {
      if (!call_with_entry(ctx, &argv[1], call, h, (int64_t)pos)) break;
      if (call[3].type == XB_T_LOGICAL && call[3].v.l) {
        xb_set_int(ret, (int64_t)pos);
        break;
      }
    }
    xb_calls_end(ctx, &frame, 4);
    return;
  }
  xb_hash_compact(h);
  for (uint64_t pos = start; pos < start + count; ++pos)
    if (xb_value_matches(ctx, &h->entries[pos - 1].value, &argv[1])) {
      xb_set_int(ret, (int64_t)pos);
      return;
    }
}

/* HEval( hHash, bBlock, [nStart], [nCount] ): calls bBlock( key, value,
 * position ) for each entry of the range still there when its turn comes;
 * returns hHash. */
void xbf_HEVAL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HEVAL", argc, argv, 0);
  if (!h) return;
  if (argc < 2 || argv[1].type != XB_T_BLOCK) {
    xb_error_in(ctx, "HEVAL", NULL, EG_ARG, 1123, "HEVAL", 0, argc, argv);
    return;
  }
  *ret = argv[0];
  uint64_t start = xb_range_start(argc, argv, 2, h->len);
  if (!start) return;
  uint64_t count = xb_range_count(argc, argv, 3, h->len - start + 1);
  xb_item call[4];
  xb_frame frame;
  xb_calls_begin(ctx, call, 4, &frame, &heval_proc);
  for (uint64_t pos = start; pos < start + count; ++pos)
    if (!call_with_entry(ctx, &argv[1], call, h, (int64_t)pos)) break;
  xb_calls_end(ctx, &frame, 4);
}

/* Room for `n` entries; past the largest hash, the fatal error of a size
 * that cannot be allocated (as for arrays). */
static void reserve(xb_hash *h, uint64_t n) {
  xb_hash_reserve(h, n > XB_HASH_MAX_LEN ? XB_HASH_MAX_LEN + 1 : (uint32_t)n);
}

/* Adds the entries of `src` from `start`, `count` of them, to `dest` (a key
 * there gets the new value). Both are reachable; a collection moves nothing,
 * so `src`'s entries stay where they are while `dest` grows. */
static void copy_entries(xb_hash *dest, xb_hash *src, uint64_t start, uint64_t count) {
  reserve(dest, (uint64_t)dest->len + count);
  xb_hash_compact(src);
  for (uint64_t k = start - 1; k < start - 1 + count; ++k)
    xb_hash_set(dest, &src->entries[k].key, &src->entries[k].value);
}

/* HClone( hHash ): a copy, the arrays and hashes inside it copied too
 * (xb_clone_deep), with the same letter case rule. */
void xbf_HCLONE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  if (hash_arg(ctx, "HCLONE", argc, argv, 0)) xb_clone_deep(ret, &argv[0]);
}

/* HCopy( hSource, hDest, [nStart], [nCount] ): adds the entries of hSource
 * in the range to hDest; returns hDest. */
void xbf_HCOPY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *src = hash_arg(ctx, "HCOPY", argc, argv, 0);
  xb_hash *dest = src ? hash_arg(ctx, "HCOPY", argc, argv, 1) : NULL;
  if (!dest) return;
  *ret = argv[1];
  uint64_t start = xb_range_start(argc, argv, 2, src->len);
  if (start) copy_entries(dest, src, start, xb_range_count(argc, argv, 3, src->len - start + 1));
}

/* HMerge( hDest, hSource, [bFilter] ): adds the entries of hSource to hDest,
 * those for which bFilter( key, value, position ) returns .T. when given;
 * returns hDest. */
void xbf_HMERGE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *dest = hash_arg(ctx, "HMERGE", argc, argv, 0);
  xb_hash *src = dest ? hash_arg(ctx, "HMERGE", argc, argv, 1) : NULL;
  if (!src) return;
  *ret = argv[0];
  if (argc < 3 || argv[2].type != XB_T_BLOCK) {
    copy_entries(dest, src, 1, src->len);
    return;
  }
  /* The block's arguments and result, then the entry as it was before the block ran: the block may replace its
   * arguments (PValue()), and the entry it was given is the one merged. */
  xb_item call[6];
  xb_frame frame;
  xb_calls_begin(ctx, call, 6, &frame, &hmerge_proc);
  for (int64_t pos = 1; pos <= (int64_t)src->len; ++pos) {
    const xb_hentry *e = xb_hash_at(src, pos);
    if (!e) break;
    call[4] = e->key;
    call[5] = e->value;
    if (!call_with_entry(ctx, &argv[2], call, src, pos)) break;
    if (call[3].type == XB_T_LOGICAL && call[3].v.l) xb_hash_set(dest, &call[4], &call[5]);
  }
  xb_calls_end(ctx, &frame, 6);
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
  if (s->h->len != s->len) {
    s->failed = 1;
    return 0;
  }
  const xb_item *x = &xb_hash_at(s->h, (int64_t)i + 1)->key, *y = &xb_hash_at(s->h, (int64_t)j + 1)->key;
  if (!s->block) return xb_value_less(s->ctx, x, y);
  s->call[0] = *x;
  s->call[1] = *y;
  if (!xb_calls_run(s->ctx, s->block, s->call, 2)) {
    s->failed = 1;
    return 0;
  }
  return xb_order_answer(&s->call[2]);
}

/* HSort( hHash, [bOrder] ): orders the entries by key, stably, by bOrder(
 * key1, key2 ) (whether the first goes first) or as ASort orders values;
 * returns hHash. */
void xbf_HSORT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HSORT", argc, argv, 0);
  if (!h) return;
  *ret = argv[0];
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
  xb_hash_clear(h);
  *ret = argv[0];
}

/* HFill( hHash, xValue ): gives every key the value; returns hHash. */
void xbf_HFILL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HFILL", argc, argv, 0);
  if (!h) return;
  xb_item nil;
  xb_set_nil(&nil);
  xb_hash_compact(h);
  for (uint32_t k = 0; k < h->len; ++k) h->entries[k].value = argc > 1 ? argv[1] : nil;
  xb_gc_write(h);
  *ret = argv[0];
}

/* HAllocate( hHash, nCount ): room for nCount entries; returns hHash. */
void xbf_HALLOCATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HALLOCATE", argc, argv, 0);
  if (!h) return;
  int64_t n = argc > 1 && xb_is_number(&argv[1]) ? xb_num_to_i64(&argv[1]) : 0;
  if (n > 0) reserve(h, (uint64_t)n);
  *ret = argv[0];
}

/* HCaseMatch( hHash, [lMatch] ): whether letter case matters in its string
 * keys (.T. for a new hash); lMatch changes it. Keys that then become equal merge
 * (xb_hash_set_flags). */
void xbf_HCASEMATCH(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_hash *h = hash_arg(ctx, "HCASEMATCH", argc, argv, 0);
  if (!h) return;
  xb_set_logical(ret, !(h->flags & XB_HASH_IGNORECASE));
  if (argc > 1 && argv[1].type == XB_T_LOGICAL)
    xb_hash_set_flags(h, argv[1].v.l ? h->flags & ~XB_HASH_IGNORECASE : h->flags | XB_HASH_IGNORECASE);
}
