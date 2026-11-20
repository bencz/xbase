/* Hashes (see xb_hash in types.h). */
#include "xbrt_int.h"

#include "core/shared.h"

#include <stdlib.h>
#include <string.h>

#define MIN_CAPACITY 8u
#define SHRINK_CAP 64u /* a store above this many entries goes when cleared, shrinks when mostly empty */

/* Flooding. A key's hash is fixed (xb_hash_bytes, the integers' fold), so anyone can choose keys whose hashes share
 * their low bits, and every probe among them walks them all: n insertions cost n^2 / 2 steps. An insertion whose
 * probe goes past FLOOD_PROBE slots, which keys that do not collide on purpose never make (the index is at most half
 * full), turns the table keyed (XB_HASH_KEYED) for good: every key's hash is computed again by SipHash-1-3 (Aumasson
 * and Bernstein's keyed function, one compression and three finalization rounds) under a key the process draws from
 * the system's randomness on first need, which no one outside it can know, and a string's own hash is not used. */
#define FLOOD_PROBE 128u

static uint64_t rotl64(uint64_t x, int b) { return (x << b) | (x >> (64 - b)); }

static void sip_round(uint64_t v[4]) {
  v[0] += v[1];
  v[1] = rotl64(v[1], 13) ^ v[0];
  v[0] = rotl64(v[0], 32);
  v[2] += v[3];
  v[3] = rotl64(v[3], 16) ^ v[2];
  v[0] += v[3];
  v[3] = rotl64(v[3], 21) ^ v[0];
  v[2] += v[1];
  v[1] = rotl64(v[1], 17) ^ v[2];
  v[2] = rotl64(v[2], 32);
}

/* SipHash-1-3 of the n bytes at p under key k, the bytes read as little-endian words (ASCII letters upper-cased with
 * `upper`), folded to 32 bits, never 0. */
static uint32_t sip_hash(const uint64_t k[2], const char *p, size_t n, int upper) {
  uint64_t v[4] = {k[0] ^ UINT64_C(0x736f6d6570736575), k[1] ^ UINT64_C(0x646f72616e646f6d),
                   k[0] ^ UINT64_C(0x6c7967656e657261), k[1] ^ UINT64_C(0x7465646279746573)};
  uint64_t last = (uint64_t)n << 56;
  for (;;) {
    size_t take = n < 8 ? n : 8;
    uint64_t m = 0;
    for (size_t i = 0; i < take; ++i) {
      unsigned char c = (unsigned char)(upper ? xb_ascii_upper(p[i]) : p[i]);
      m |= (uint64_t)c << (8 * i);
    }
    if (take < 8) m |= last; /* the final word: the rest and the length's low byte */
    v[3] ^= m;
    sip_round(v);
    v[0] ^= m;
    p += take;
    n -= take;
    if (take < 8) break;
  }
  v[2] ^= 0xff;
  for (int r = 0; r < 3; ++r) sip_round(v);
  uint64_t h = v[0] ^ v[1] ^ v[2] ^ v[3];
  uint32_t r = (uint32_t)(h ^ (h >> 32));
  return r ? r : 1u;
}

/* The process's key: drawn once, by the first thread that needs it (the others wait for it). */
static uint64_t flood_key[2];
static uint32_t flood_key_state; /* 0: not drawn, 1: being drawn, 2: ready */

static const uint64_t *keyed_key(void) {
  if (xb_atomic_load32(&flood_key_state, XB_ACQUIRE) == 2) return flood_key;
  uint32_t none = 0;
  if (xb_atomic_cas32(&flood_key_state, &none, 1, XB_ACQ_REL)) {
    xb_platform_random(flood_key, sizeof flood_key);
    xb_atomic_store32(&flood_key_state, 2, XB_RELEASE);
  }
  while (xb_atomic_load32(&flood_key_state, XB_ACQUIRE) != 2) xb_platform_sleep_ms(0);
  return flood_key;
}

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

/* A key's hash in a keyed table: SipHash of its bytes, of an integer's 8 bytes, of a date's or a double's 8 bytes
 * and a byte that tells which (little-endian whatever the target). */
static uint32_t keyed_hash(const xb_hash *h, const xb_item *k) {
  const uint64_t *key = keyed_key();
  if (k->type == XB_T_STRING) return sip_hash(key, k->v.s->data, k->v.s->len, (h->flags & XB_HASH_IGNORECASE) != 0);
  int64_t i;
  uint64_t bits;
  char kind = 'D';
  if (k->type == XB_T_DATE) {
    bits = (uint64_t)k->v.i;
  } else if (integral_key(k, &i)) {
    bits = (uint64_t)i;
    kind = 'I';
  } else {
    memcpy(&bits, &k->v.d, sizeof bits);
    kind = 'F';
  }
  char bytes[9];
  for (int b = 0; b < 8; ++b) bytes[b] = (char)(bits >> (8 * b));
  bytes[8] = kind;
  return sip_hash(key, bytes, sizeof bytes, 0);
}

static uint32_t key_hash(const xb_hash *h, const xb_item *k) {
  if (h->flags & XB_HASH_KEYED) return keyed_hash(h, k);
  if (k->type == XB_T_STRING) {
    xb_str *s = k->v.s;
    if (h->flags & XB_HASH_IGNORECASE) return xb_hash_bytes_as(s->data, s->len, 1);
    /* a string an append grows keeps its room in `hash` (only its variable sees it, so it never gets here) */
    int growing = (s->flags & XB_STR_APPEND) != 0;
    /* Threads may compute it at once: each stores the same value, atomically. */
    uint32_t known = growing ? 0 : xb_atomic_load32(&s->hash, XB_RELAXED);
    if (known) return known; /* literals carry theirs */
    uint32_t v = xb_hash_bytes(s->data, s->len);
    if (!growing && !(s->flags & XB_STR_STATIC)) xb_atomic_store32(&s->hash, v, XB_RELAXED); /* strings are immutable */
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

static int key_kind(const xb_item *k) { return k->type == XB_T_STRING ? 2 : k->type == XB_T_DATE; }

int xb_hash_key_order(const xb_hash *h, const xb_item *a, const xb_item *b) {
  int ka = key_kind(a), kb = key_kind(b);
  if (ka != kb) return ka < kb ? -1 : 1;
  if (ka == 1) return (a->v.i > b->v.i) - (a->v.i < b->v.i);
  if (ka == 0) {
    if (a->type == XB_T_INT && b->type == XB_T_INT) return (a->v.i > b->v.i) - (a->v.i < b->v.i);
    return key_equal(h, a, b) ? 0 : xb_order_double(xb_num_value(a), xb_num_value(b));
  }
  const xb_str *x = a->v.s, *y = b->v.s;
  size_t n = x->len < y->len ? x->len : y->len;
  for (size_t i = 0; i < n; ++i) {
    unsigned char p = (unsigned char)x->data[i], q = (unsigned char)y->data[i];
    if (h->flags & XB_HASH_IGNORECASE) {
      p = (unsigned char)xb_ascii_upper((char)p);
      q = (unsigned char)xb_ascii_upper((char)q);
    }
    if (p != q) return p < q ? -1 : 1;
  }
  return (x->len > y->len) - (x->len < y->len);
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

/* A store for `cap` entries: NIL items, free slots; `fallible`: NULL when it is past the largest hash or no memory
 * holds it (a size the program chose). */
static xb_hentry *new_store_of(uint64_t cap, int fallible) {
  uint64_t slots = 2 * cap;
  /* computed in 64 bits and checked before it is narrowed to size_t (32-bit hosts) */
  uint64_t size = cap * (sizeof(xb_hentry) + 2 * sizeof(uint32_t)) + slots * sizeof(uint32_t);
  if (cap > XB_HASH_MAX_LEN || size > XB_STR_MAX_LEN) {
    if (fallible) return NULL;
    xb_gc_fatal("out of memory");
  }
  size_t bytes = (size_t)size;
  xb_hentry *entries = fallible ? xb_gc_try_alloc(XB_GC_HSTORE, bytes) : xb_gc_alloc(XB_GC_HSTORE, bytes);
  if (entries) memset(entries, 0, bytes);
  return entries;
}

static xb_hentry *new_store(uint32_t cap) { return new_store_of(cap, 0); }

/* The live entries moved into `entries`, a new store for `cap` of them; nothing allocates. */
static void move_into(xb_hash *h, xb_hentry *entries, uint32_t cap) {
  uint32_t slots = 2 * cap;
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

/* A new store for `cap` entries holding the live ones; `h` is reachable. */
static void restore(xb_hash *h, uint32_t cap) { move_into(h, new_store(cap), cap); }

void xb_hash_reserve(xb_hash *h, uint32_t n) {
  if (n <= h->cap) return;
  uint32_t cap = h->cap ? h->cap : MIN_CAPACITY;
  while (cap < n) cap *= 2;
  restore(h, cap);
}

/* The live entries of `src` into `dst`, empty with room for them; nothing allocates. */
static void copy_into(xb_hash *dst, const xb_hash *src) {
  dst->flags = src->flags;
  if (!src->len) return;
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

void xb_hash_copy(xb_hash *dst, const xb_hash *src) {
  xb_hash_reserve(dst, src->len);
  copy_into(dst, src);
}

/* Room for one more entry: the holes compacted away when they are a quarter
 * of the store, else a store twice as large. */
static void make_room(xb_hash *h) {
  if (h->used < h->cap) return;
  if (h->cap && h->used - h->len >= h->cap / 4) xb_hash_compact(h);
  else xb_hash_reserve(h, h->cap ? h->cap * 2 : MIN_CAPACITY);
}

/* A new key, room made for it; a probe past FLOOD_PROBE slots turns the table keyed. */
static void add_entry(xb_hash *h, const xb_item *key, const xb_item *value, uint32_t hv) {
  uint32_t e = h->used++;
  h->entries[e].key = *key;
  h->entries[e].value = *value;
  h->hashes[e] = hv;
  uint32_t slot = slot_of(h, key, hv);
  h->index[slot] = e + 1;
  ++h->len;
  if (h->tree) tree_update(h, e, 1);
  xb_gc_write(h);
  if (((slot - hv) & h->imask) > FLOOD_PROBE && !(h->flags & XB_HASH_KEYED)) {
    h->flags |= XB_HASH_KEYED;
    xb_hash_reindex(h);
  }
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
  add_entry(h, key, value, hv);
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

/* Emptied: a store larger than SHRINK_CAP entries is dropped (the next key makes a small one), so clearing costs what
 * the hash holds now and a peak's room goes back; a small one is kept, emptied. */
void xb_hash_clear(xb_hash *h) {
  h->tree = NULL;
  if (h->cap > SHRINK_CAP) {
    h->entries = NULL;
    h->hashes = h->index = NULL;
    h->cap = h->imask = 0;
    h->len = h->used = 0;
    xb_gc_write(h);
    return;
  }
  for (uint32_t e = 0; e < h->used; ++e) {
    xb_set_nil(&h->entries[e].key);
    xb_set_nil(&h->entries[e].value);
  }
  h->len = h->used = 0;
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

/* Shared hashes (core/shared.h): every operation runs under the hash's lock, reads too (one may reorganize it: the
 * position tree, a compaction), so nothing is read under its sequence counter; values and keys come out copied. A
 * store a key needs is allocated before the lock is taken (rooted meanwhile) and taken under it when still needed. */
static int lock_if_shared(const xb_hash *h) {
  int shared = xb_is_shared(h);
  if (shared) xb_seq_lock(h);
  return shared;
}

static void unlock_if(const xb_hash *h, int shared) {
  if (shared) xb_seq_unlock(h);
}

uint32_t xb_hash_length(const xb_hash *h) {
  int shared = lock_if_shared(h);
  uint32_t n = h->len;
  unlock_if(h, shared);
  return n;
}

int xb_hash_get(xb_hash *h, const xb_item *key, xb_item *value) { return xb_hash_lookup(h, key, NULL, value); }

int xb_hash_lookup(xb_hash *h, const xb_item *key, xb_item *stored, xb_item *value) {
  int shared = lock_if_shared(h);
  int64_t e = xb_hash_find(h, key);
  if (e >= 0 && stored) *stored = h->entries[e].key;
  if (e >= 0 && value) *value = h->entries[e].value;
  unlock_if(h, shared);
  return e >= 0;
}

int64_t xb_hash_position(xb_hash *h, const xb_item *key) {
  int shared = lock_if_shared(h);
  int64_t pos = xb_hash_pos(h, key);
  unlock_if(h, shared);
  return pos;
}

int xb_hash_entry(xb_hash *h, int64_t pos, xb_item *key, xb_item *value) {
  int shared = lock_if_shared(h);
  const xb_hentry *e = xb_hash_at(h, pos);
  if (e && key) *key = e->key;
  if (e && value) *value = e->value;
  unlock_if(h, shared);
  return e != NULL;
}

int xb_hash_value_at(xb_hash *h, int64_t pos, const xb_item *v, xb_item *now) {
  if (v && xb_is_shared(h)) xb_share(v);
  int shared = lock_if_shared(h);
  xb_hentry *e = xb_hash_at(h, pos);
  if (e && v) {
    e->value = *v;
    xb_gc_write(h);
  }
  if (e && now) *now = e->value;
  unlock_if(h, shared);
  return e != NULL;
}

int xb_hash_replace(xb_hash *h, const xb_item *key, const xb_item *v) {
  if (xb_is_shared(h)) xb_share(v);
  int shared = lock_if_shared(h);
  int64_t e = xb_hash_find(h, key);
  if (e >= 0) {
    h->entries[e].value = *v;
    xb_gc_write(h);
  }
  unlock_if(h, shared);
  return e >= 0;
}

static uint32_t grown(uint32_t cap, uint32_t need);

/* A local hash left with an eighth of its store or less (above SHRINK_CAP entries) moves into a store twice as large
 * as it needs: a cache whose keys came and went gives its peak's room back. The allocation is a safepoint, after which
 * a hash that became shared meanwhile is left as it is. */
static void local_shrink(xb_hash *h) {
  if (h->cap <= SHRINK_CAP || h->len > h->cap / 8) return;
  uint32_t want = grown(0, 2 * h->len);
  xb_hentry *store = new_store(want); /* `h` is reachable; the store, until it is in place, by the stack's pin */
  if (!xb_is_shared(h)) move_into(h, store, want);
}

int xb_hash_remove(xb_hash *h, const xb_item *key) {
  int shared = lock_if_shared(h);
  int found = xb_hash_del(h, key);
  unlock_if(h, shared);
  if (found && !shared) local_shrink(h);
  return found;
}

int xb_hash_remove_at(xb_hash *h, int64_t pos) {
  int shared = lock_if_shared(h);
  int found = xb_hash_at(h, pos) != NULL;
  if (found) xb_hash_del_at(h, pos);
  unlock_if(h, shared);
  if (found && !shared) local_shrink(h);
  return found;
}

void xb_hash_empty(xb_hash *h) {
  int shared = lock_if_shared(h);
  xb_hash_clear(h);
  unlock_if(h, shared);
}

void xb_hash_fill(xb_hash *h, const xb_item *v) {
  if (xb_is_shared(h)) xb_share(v);
  int shared = lock_if_shared(h);
  xb_hash_compact(h);
  for (uint32_t k = 0; k < h->len; ++k) h->entries[k].value = *v;
  xb_gc_write(h);
  unlock_if(h, shared);
}

int xb_hash_case_match(xb_hash *h, int change, int match) {
  int shared = lock_if_shared(h);
  int was = !(h->flags & XB_HASH_IGNORECASE);
  if (change) xb_hash_set_flags(h, match ? h->flags & ~XB_HASH_IGNORECASE : h->flags | XB_HASH_IGNORECASE);
  unlock_if(h, shared);
  return was;
}

/* The capacity a store of `cap` grows to for `need` entries. */
static uint32_t grown(uint32_t cap, uint32_t need) {
  uint32_t want = cap ? cap : MIN_CAPACITY;
  while (want < need) want *= 2;
  return want;
}

/* Room for one more entry in a local hash, made in place: a store that must grow is made first, an allocation and so a
 * safepoint, where the program's storage may become shared (publication, docs/concurrency.md) and this hash with it,
 * so it is tested again after it: 0 then, nothing changed, for the shared path to take over. */
static int local_entry_room(xb_hash *h) {
  if (h->used < h->cap || (h->cap && h->used - h->len >= h->cap / 4)) return !xb_is_shared(h); /* none to make */
  uint32_t want = grown(h->cap, h->cap * 2);
  xb_hentry *store = new_store(want); /* `h` is reachable; the store, until it is in place, by the stack's pin */
  if (xb_is_shared(h)) return 0;
  move_into(h, store, want);
  return 1;
}

void xb_hash_put(xb_ctx *ctx, xb_hash *h, const xb_item *key, const xb_item *value) {
  if (!xb_is_shared(h) && local_entry_room(h)) {
    xb_hash_set(h, key, value); /* room made: nothing allocates */
    return;
  }
  xb_share(key);
  xb_share(value);
  xb_item spare;
  xb_set_nil(&spare);
  xb_gc_root_push(ctx, &spare);
  uint32_t room = 0;
  for (;;) {
    xb_seq_lock(h);
    uint32_t hv = key_hash(h, key); /* under the lock: the letter case rule may change */
    uint32_t e = h->len ? h->index[slot_of(h, key, hv)] : 0;
    if (e) {
      h->entries[e - 1].value = *value;
      xb_gc_write(h);
    } else {
      if (h->used == h->cap && !(h->cap && h->used - h->len >= h->cap / 4)) { /* holes enough to compact: no store */
        uint32_t want = grown(h->cap, h->cap * 2);
        if (room < want) {
          xb_seq_unlock(h);
          room = want;
          /* The tag only keeps it alive: a store is traced through its hash. */
          xb_set_ptr(&spare, XB_T_HASH, new_store(room));
          continue;
        }
        move_into(h, spare.v.p, room);
      }
      make_room(h); /* a compaction at most */
      add_entry(h, key, value, hv);
    }
    xb_seq_unlock(h);
    break;
  }
  xb_gc_root_pop(ctx, 1);
}

static int reserve_any(xb_ctx *ctx, xb_hash *h, uint64_t n, int fallible) {
  if (n > XB_HASH_MAX_LEN) {
    if (fallible) return 0;
    xb_gc_fatal("out of memory");
  }
  if (!xb_is_shared(h)) {
    if (n <= h->cap) return 1;
    uint32_t want = grown(h->cap, (uint32_t)n);
    xb_hentry *store = new_store_of(want, fallible); /* a safepoint: tested again after it, as in local_entry_room */
    if (!store) return 0;
    if (!xb_is_shared(h)) {
      move_into(h, store, want);
      return 1;
    }
  }
  xb_item spare;
  xb_set_nil(&spare);
  xb_gc_root_push(ctx, &spare);
  uint32_t room = 0;
  for (;;) {
    xb_seq_lock(h);
    if (n > h->cap) {
      uint32_t want = grown(h->cap, (uint32_t)n);
      if (room < want) {
        xb_seq_unlock(h);
        room = want;
        xb_hentry *store = new_store_of(room, fallible);
        if (!store) {
          xb_gc_root_pop(ctx, 1);
          return 0;
        }
        xb_set_ptr(&spare, XB_T_HASH, store);
        continue;
      }
      move_into(h, spare.v.p, room);
    }
    xb_seq_unlock(h);
    break;
  }
  xb_gc_root_pop(ctx, 1);
  return 1;
}

void xb_hash_reserve_any(xb_ctx *ctx, xb_hash *h, uint64_t n) { reserve_any(ctx, h, n, 0); }

int xb_hash_reserve_try(xb_ctx *ctx, xb_hash *h, uint64_t n) { return reserve_any(ctx, h, n, 1); }

void xb_hash_copy_any(xb_hash *dst, xb_hash *src) {
  if (!xb_is_shared(src)) {
    xb_hash_copy(dst, src);
    return;
  }
  for (;;) { /* room for the entries seen, then all of them in one locked copy when they still fit */
    xb_hash_reserve(dst, xb_hash_length(src));
    xb_seq_lock(src);
    int fits = src->len <= dst->cap;
    if (fits) copy_into(dst, src);
    xb_seq_unlock(src);
    if (fits) return;
  }
}

void xb_hash_snapshot(xb_item *out, xb_hash *h) {
  xb_set_hash(out, xb_hash_alloc());
  xb_hash_copy_any(out->v.p, h);
}

void xb_hash_take_order(xb_hash *h, const xb_hash *sorted) {
  int shared = lock_if_shared(h);
  uint32_t n = h->len;
  if (n == sorted->len && n) {
    /* The order only: each key of the copy found in the hash as it is now (with its letter case rule now), once, its
     * entry as it is now; anything else (a key gone, two keys one under a new rule) leaves the hash as it is. */
    xb_hentry *order = malloc(n * sizeof *order);
    uint8_t *taken = calloc(h->used, 1);
    if (!order || !taken) xb_gc_fatal("out of memory");
    int same = 1;
    for (uint32_t k = 0; same && k < n; ++k) {
      int64_t e = xb_hash_find(h, &sorted->entries[k].key); /* `sorted` is compacted: positions are entries */
      same = e >= 0 && !taken[e];
      if (same) {
        taken[e] = 1;
        order[k] = h->entries[e];
      }
    }
    if (same) {
      xb_hash_compact(h);
      memcpy(h->entries, order, n * sizeof *order);
      xb_hash_reindex(h);
      xb_gc_write(h);
    }
    free(order);
    free(taken);
  }
  unlock_if(h, shared);
}
