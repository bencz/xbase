/* Hashes (see xb_hash in types.h). */
#include "xbrt_int.h"

#include <string.h>

#define MIN_CAPACITY 8u

void xb_set_hash(xb_item *it, xb_hash *h) { xb_set_ptr(it, XB_T_HASH, h); }

xb_hash *xb_hash_alloc(void) {
  xb_hash *h = xb_gc_alloc(XB_GC_HASH, sizeof(xb_hash));
  memset(h, 0, sizeof *h);
  return h;
}

int xb_hash_key_ok(const xb_item *key) {
  return key->type == XB_T_STRING || xb_is_number(key) || key->type == XB_T_DATE;
}

/* A number key as an integer when it equals one (1.0 is 1, -0.0 is 0). */
static int integral_key(const xb_item *n, int64_t *i) {
  if (n->type == XB_T_INT) {
    *i = n->v.i;
    return 1;
  }
  double d = n->v.d;
  if (!xb_dbl_fits_i64(d) || (double)(int64_t)d != d) return 0;
  *i = (int64_t)d;
  return 1;
}

static uint32_t fold(uint64_t x) {
  x = xb_hash_mix64(x);
  return (uint32_t)(x ^ (x >> 32));
}

static uint32_t key_hash(const xb_hash *h, const xb_item *k) {
  if (k->type == XB_T_STRING) {
    xb_str *s = k->v.s;
    if (h->flags & XB_HASH_IGNORECASE) return xb_hash_bytes_as(s->data, s->len, 1);
    if (s->hash) return s->hash;
    uint32_t v = xb_hash_bytes(s->data, s->len);
    if (!(s->flags & XB_STR_STATIC)) s->hash = v; /* strings are immutable: the hash holds */
    return v;
  }
  if (k->type == XB_T_DATE) return fold((uint64_t)k->v.i ^ UINT64_C(0xd6e8feb86659fd93));
  int64_t i;
  if (integral_key(k, &i)) return fold((uint64_t)i);
  uint64_t bits;
  memcpy(&bits, &k->v.d, sizeof bits);
  return fold(bits);
}

static int key_equal(const xb_hash *h, const xb_item *a, const xb_item *b) {
  if (a->type == XB_T_STRING || b->type == XB_T_STRING) {
    if (a->type != b->type) return 0;
    const xb_str *x = a->v.s, *y = b->v.s;
    if (x->len != y->len) return 0;
    if (!(h->flags & XB_HASH_IGNORECASE)) return memcmp(x->data, y->data, x->len) == 0;
    for (size_t i = 0; i < x->len; ++i)
      if (xb_ascii_upper(x->data[i]) != xb_ascii_upper(y->data[i])) return 0;
    return 1;
  }
  if (a->type == XB_T_DATE || b->type == XB_T_DATE) return a->type == b->type && a->v.i == b->v.i;
  int64_t i, j;
  int ia = integral_key(a, &i), ib = integral_key(b, &j);
  if (ia || ib) return ia && ib && i == j;
  return memcmp(&a->v.d, &b->v.d, sizeof a->v.d) == 0;
}

/* The index slot of `key`: its entry's, or the free slot where it would go. */
static uint32_t slot_of(const xb_hash *h, const xb_item *key, uint32_t hv) {
  for (uint32_t i = hv & h->imask;; i = (i + 1) & h->imask) {
    uint32_t e = h->index[i];
    if (!e || (h->hashes[e - 1] == hv && key_equal(h, &h->entries[e - 1].key, key))) return i;
  }
}

static void index_entries(xb_hash *h) {
  memset(h->index, 0, ((size_t)h->imask + 1) * sizeof *h->index);
  for (uint32_t e = 0; e < h->used; ++e) {
    if (h->entries[e].key.type == XB_T_NIL) continue;
    uint32_t i = h->hashes[e] & h->imask;
    while (h->index[i]) i = (i + 1) & h->imask;
    h->index[i] = e + 1;
  }
}

/* The Fenwick tree over the entries: tree[i - 1] counts the live entries in
 * (i - lowbit(i), i], so a prefix count or a search for a position visits
 * log2(cap) nodes. */
static uint32_t lowbit(uint32_t i) { return i & (0u - i); }

static void tree_build(xb_hash *h) {
  uint32_t *t = h->index + h->imask + 1;
  for (uint32_t i = 0; i < h->cap; ++i) t[i] = i < h->used && h->entries[i].key.type != XB_T_NIL;
  for (uint32_t i = 1; i <= h->cap; ++i)
    if (i + lowbit(i) <= h->cap) t[i + lowbit(i) - 1] += t[i - 1];
  h->tree = t;
}

/* Entry `e` became live, or a hole. */
static void tree_update(xb_hash *h, uint32_t e, int live) {
  for (uint32_t i = e + 1; i <= h->cap; i += lowbit(i)) {
    if (live) ++h->tree[i - 1];
    else --h->tree[i - 1];
  }
}

/* The entry at position `pos` (1 .. len). */
static uint32_t tree_find(const xb_hash *h, uint32_t pos) {
  uint32_t at = 0;
  for (uint32_t step = h->cap; step; step >>= 1)
    if (at + step <= h->cap && h->tree[at + step - 1] < pos) {
      at += step;
      pos -= h->tree[at - 1];
    }
  return at;
}

/* The position of the live entry `e`: the live entries up to it. */
static uint32_t tree_position(const xb_hash *h, uint32_t e) {
  uint32_t n = 0;
  for (uint32_t i = e + 1; i; i -= lowbit(i)) n += h->tree[i - 1];
  return n;
}

void xb_hash_compact(xb_hash *h) {
  if (h->used == h->len) return;
  uint32_t to = 0;
  for (uint32_t e = 0; e < h->used; ++e) {
    if (h->entries[e].key.type == XB_T_NIL) continue;
    h->entries[to] = h->entries[e];
    h->hashes[to] = h->hashes[e];
    ++to;
  }
  for (uint32_t e = to; e < h->used; ++e) {
    xb_set_nil(&h->entries[e].key);
    xb_set_nil(&h->entries[e].value);
  }
  h->used = to;
  h->tree = NULL;
  index_entries(h);
}

/* A new store for `cap` entries holding the live ones; `h` is reachable. */
static void restore(xb_hash *h, uint32_t cap) {
  uint32_t slots = 2 * cap;
  /* computed in 64 bits and checked before it is narrowed to size_t (32-bit hosts) */
  uint64_t size = (uint64_t)cap * (sizeof(xb_hentry) + 2 * sizeof(uint32_t)) + (uint64_t)slots * sizeof(uint32_t);
  if (cap > XB_HASH_MAX_LEN || size > XB_STR_MAX_LEN) xb_gc_fatal("out of memory");
  size_t bytes = (size_t)size;
  xb_hentry *entries = xb_gc_alloc(XB_GC_HSTORE, bytes);
  memset(entries, 0, bytes); /* NIL items, free slots */
  uint32_t *hashes = (uint32_t *)(entries + cap);
  uint32_t n = 0;
  for (uint32_t e = 0; e < h->used; ++e) {
    if (h->entries[e].key.type == XB_T_NIL) continue;
    entries[n] = h->entries[e];
    hashes[n] = h->hashes[e];
    ++n;
  }
  h->entries = entries;
  h->hashes = hashes;
  h->index = hashes + cap;
  h->tree = NULL;
  h->cap = cap;
  h->imask = slots - 1;
  h->used = n;
  index_entries(h);
  xb_gc_write(h);
}

void xb_hash_reserve(xb_hash *h, uint32_t n) {
  if (n <= h->cap) return;
  uint32_t cap = h->cap ? h->cap : MIN_CAPACITY;
  while (cap < n) cap *= 2;
  restore(h, cap);
}

void xb_hash_copy(xb_hash *dst, const xb_hash *src) {
  dst->flags = src->flags;
  if (!src->len) return;
  xb_hash_reserve(dst, src->len);
  uint32_t n = 0;
  for (uint32_t e = 0; e < src->used; ++e) {
    if (src->entries[e].key.type == XB_T_NIL) continue;
    dst->entries[n] = src->entries[e];
    dst->hashes[n] = src->hashes[e];
    ++n;
  }
  dst->len = dst->used = n;
  index_entries(dst);
  xb_gc_write(dst);
}

/* Room for one more entry: the holes compacted away when they are a quarter
 * of the store, else a store twice as large. */
static void make_room(xb_hash *h) {
  if (h->used < h->cap) return;
  if (h->cap && h->used - h->len >= h->cap / 4) xb_hash_compact(h);
  else xb_hash_reserve(h, h->cap ? h->cap * 2 : MIN_CAPACITY);
}

int64_t xb_hash_find(const xb_hash *h, const xb_item *key) {
  if (!h->len) return -1;
  uint32_t e = h->index[slot_of(h, key, key_hash(h, key))];
  return e ? (int64_t)e - 1 : -1;
}

void xb_hash_set(xb_hash *h, const xb_item *key, const xb_item *value) {
  uint32_t hv = key_hash(h, key);
  if (h->len) {
    uint32_t e = h->index[slot_of(h, key, hv)];
    if (e) {
      h->entries[e - 1].value = *value;
      xb_gc_write(h);
      return;
    }
  }
  make_room(h);
  uint32_t e = h->used++;
  h->entries[e].key = *key;
  h->entries[e].value = *value;
  h->hashes[e] = hv;
  h->index[slot_of(h, key, hv)] = e + 1;
  ++h->len;
  if (h->tree) tree_update(h, e, 1);
  xb_gc_write(h);
}

/* Frees index slot `i`, moving back the slots after it that probing would no
 * longer reach (deletion without tombstones). */
static void free_slot(xb_hash *h, uint32_t i) {
  for (uint32_t j = (i + 1) & h->imask; h->index[j]; j = (j + 1) & h->imask) {
    uint32_t home = h->hashes[h->index[j] - 1] & h->imask;
    int reachable = i <= j ? (home > i && home <= j) : (home > i || home <= j);
    if (reachable) continue;
    h->index[i] = h->index[j];
    i = j;
  }
  h->index[i] = 0;
}

static void remove_entry(xb_hash *h, uint32_t slot) {
  uint32_t e = h->index[slot] - 1;
  xb_set_nil(&h->entries[e].key);
  xb_set_nil(&h->entries[e].value);
  free_slot(h, slot);
  --h->len;
  if (h->tree) tree_update(h, e, 0);
  if (e + 1 == h->used) --h->used; /* the last entry: no hole */
}

int xb_hash_del(xb_hash *h, const xb_item *key) {
  if (!h->len) return 0;
  uint32_t slot = slot_of(h, key, key_hash(h, key));
  if (!h->index[slot]) return 0;
  remove_entry(h, slot);
  return 1;
}

xb_hentry *xb_hash_at(xb_hash *h, int64_t pos) {
  if (pos < 1 || (uint64_t)pos > h->len) return NULL;
  if (h->used == h->len) return &h->entries[pos - 1];
  if (!h->tree) tree_build(h);
  return &h->entries[tree_find(h, (uint32_t)pos)];
}

int64_t xb_hash_pos(xb_hash *h, const xb_item *key) {
  int64_t e = xb_hash_find(h, key);
  if (e < 0 || h->used == h->len) return e + 1;
  if (!h->tree) tree_build(h);
  return tree_position(h, (uint32_t)e);
}

/* A hash literal: `n` key and value pairs. */
void xb_hash_new(xb_ctx *ctx, xb_item *r, uint32_t n, const xb_item *kv) {
  xb_hash *h = xb_hash_alloc();
  xb_set_hash(r, h); /* `r` is a root; the pairs are frame slots */
  xb_hash_reserve(h, n);
  for (uint32_t k = 0; k < n; ++k) {
    if (!xb_hash_key_ok(&kv[2 * k])) {
      xb_item args[3] = {*r, kv[2 * k], kv[2 * k + 1]};
      xb_error(ctx, NULL, EG_BOUND, 1133, "array assign", 0, 3, args);
      return;
    }
    xb_hash_set(h, &kv[2 * k], &kv[2 * k + 1]);
  }
}

void xb_hash_del_at(xb_hash *h, int64_t pos) {
  xb_hentry *e = xb_hash_at(h, pos);
  if (!e) return;
  uint32_t hv = h->hashes[e - h->entries];
  remove_entry(h, slot_of(h, &e->key, hv));
}

void xb_hash_clear(xb_hash *h) {
  for (uint32_t e = 0; e < h->used; ++e) {
    xb_set_nil(&h->entries[e].key);
    xb_set_nil(&h->entries[e].value);
  }
  h->len = h->used = 0;
  h->tree = NULL;
  if (h->cap) memset(h->index, 0, ((size_t)h->imask + 1) * sizeof *h->index);
}

void xb_hash_set_flags(xb_hash *h, uint32_t flags) {
  if (h->flags == flags) return;
  h->flags = flags;
  if (!h->cap) return;
  h->tree = NULL; /* the merges below leave holes it would not count */
  memset(h->index, 0, ((size_t)h->imask + 1) * sizeof *h->index);
  for (uint32_t e = 0; e < h->used; ++e) {
    xb_hentry *entry = &h->entries[e];
    if (entry->key.type == XB_T_NIL) continue;
    uint32_t hv = key_hash(h, &entry->key);
    h->hashes[e] = hv;
    uint32_t i = slot_of(h, &entry->key, hv);
    if (!h->index[i]) {
      h->index[i] = e + 1;
      continue;
    }
    h->entries[h->index[i] - 1].value = entry->value; /* keys that became one: the last value wins */
    xb_set_nil(&entry->key);
    xb_set_nil(&entry->value);
    --h->len;
  }
  xb_hash_compact(h);
}

void xb_hash_reindex(xb_hash *h) {
  for (uint32_t e = 0; e < h->used; ++e) h->hashes[e] = key_hash(h, &h->entries[e].key);
  if (h->cap) index_entries(h);
}
