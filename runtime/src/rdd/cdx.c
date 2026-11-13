/* CDX order files (FoxPro's compound indexes as the reference writes them; rdd/index.h). A file is a bag of tags,
 * each a B-tree of 512-byte pages; a directory tag lists them by name.
 *
 *   a tag header, 1024 bytes: 0 the root page's offset, 4 the free page list's head (offset 0's header only), 8 a
 *     counter of changes (big-endian; offset 0's header only), 12 the key length, 14 options (1 unique, 2 partial,
 *     4 custom, 8 FOR condition, 0x20 compact, 0x40 compound, 0x80 the directory), 15 a signature (1), 16 the
 *     header's length (1024), 18 the page's (512), 20 "RCHB" (offset 0's only), 0x1F6 descending (16 bits), 0x1F8
 *     the FOR text's position and length + 1, 0x1FC the key text's, 0x200 the texts (key, NUL, FOR, NUL); every
 *     other field little-endian and zero;
 *   a page: 0 its kind (1 root, 2 leaf), 2 its entry count, 4 and 8 the pages left and right of it on its level
 *     (0xFFFFFFFF: none);
 *     a branch: entries from 12 of the key, the record number and the child page (both big-endian): each the last
 *       entry of its child's tree;
 *     a leaf: 12 its free bytes, 14 the record number's mask, 18 and 19 the repeated and trailing counts' masks,
 *       20-22 their bit widths, 23 a descriptor's bytes R; from 24 a descriptor per entry, R bytes little-endian: the
 *       record number, then the count of bytes it repeats from the entry before it in the page, then the count of
 *       trailing fill bytes (blanks for character keys, zeros else) left out; the bytes left of each key packed from
 *       the page's end downwards, zeros between;
 *   a free page: the next free page's offset, then zeros.
 *
 * Keys compare as bytes, ascending, then by record number; a descending tag is stored the same way and walked from
 * its end. A character key is its bytes padded with blanks; a number (and a date's day number) a big-endian double
 * with its sign bit flipped (a negative one wholly inverted); a logical T or F. The directory tag's keys are the tag
 * names (10 bytes), its record numbers the tags' header offsets. */
#include "rdd/index.h"
#include "rdd/pages.h"

#include "rdd/rdd.h"

#include "core/bytes.h"
#include "core/file.h"
#include "core/shared_file.h"

#include <stdlib.h>
#include <string.h>

#define CDX_PAGE 512
#define CDX_HEADER 1024
#define CDX_NONE 0xFFFFFFFFu
#define CDX_KEY_MAX 240           /* the longest key a 512-byte page takes */
#define CDX_NAME XB_INDEX_NAME_MAX /* a tag name's bytes in the directory */
#define CDX_LEAF_HEAD 24
#define CDX_BRANCH_HEAD 12
#define CDX_BRANCH_TAIL 8         /* a branch entry's record number and child after its key */
#define CDX_ENTRIES 170           /* more than a page holds (one over while it is split) */
#define CDX_CACHE 2048            /* pages kept per file at most (rdd/pages.h), made as pages come: */
#define CDX_CACHE_MIN 512         /* this many always, */
#define CDX_CACHE_BYTES (8u << 20) /* more while their decoded nodes (a node, its keys whole) take less than this */
#define CDX_ROOT 1
#define CDX_LEAF 2
#define CDX_OPT_UNIQUE 0x01
#define CDX_OPT_FOR 0x08
#define CDX_OPT_COMPACT 0x20
#define CDX_OPT_COMPOUND 0x40
#define CDX_OPT_DIRECTORY 0x80
#define CDX_SIGNATURE 1
#define CDX_FREE_HEAD 4           /* offset 0's header: the free list's head, then the counter */
#define CDX_COUNTER 8
#define CDX_KEY_LEN 12
#define CDX_OPTIONS 14
#define CDX_SIG 15
#define CDX_HEADER_LEN 16
#define CDX_PAGE_LEN 18
#define CDX_MARK 20
#define CDX_DESCEND 0x1F6
#define CDX_FOR_POS 0x1F8
#define CDX_FOR_LEN 0x1FA
#define CDX_KEY_POS 0x1FC
#define CDX_KEY_LEN_TEXT 0x1FE
#define CDX_TEXT 0x200
#define CDX_TEXT_MAX 512
#define CDX_SIGN 0x80             /* the sign bit of a number's first byte */
#define CDX_OPT_PARTIAL 0x02
#define CDX_TEXT_ROOM 510         /* the key's and the FOR condition's texts together */
#define CDX_DEPTH 64              /* the most levels a tree is followed down */
#define CDX_BALANCE 3             /* the pages a balancing works on around the one that changed */
#define CDX_WINDOW 8              /* the most pages one balancing handles */

static const char cdx_mark[4] = {'R', 'C', 'H', 'B'};

/* A page in memory: its bytes, and decoded its entries. */
typedef struct node {
  uint32_t offset;           /* 0: a free slot */
  uint16_t self;             /* its slot in the cache (rdd/pages.h; 0: a page outside it, being built) */
  int dirty;
  int pending;               /* a leaf's entries changed since its bytes were made from them */
  int full;                  /* its entries are decoded (else only its header: the bytes hold the entries) */
  const xb_tag *tag;         /* whose keys the entries are (their length and fill byte) */
  unsigned char raw[CDX_PAGE + CDX_KEY_MAX + 8]; /* a branch holds an entry over while it is split */
  uint32_t kind, n;
  uint32_t left, right;
  /* a leaf */
  int32_t space;             /* its free bytes */
  uint32_t recMask;
  unsigned char rb, db, tb, r;
  unsigned char dup[CDX_ENTRIES], trl[CDX_ENTRIES];
  /* both */
  uint32_t rec[CDX_ENTRIES], child[CDX_ENTRIES];
  unsigned char *keys;       /* n keys of the tag's length, whole (malloc'ed, `cap` bytes: a slot serves any tag) */
  size_t cap;
} node;

struct xb_tag {
  xb_bag *bag;
  char name[CDX_NAME + 1];
  uint32_t header, root;
  uint32_t keyLen;
  unsigned char options;
  unsigned char fill;        /* the trailing byte left out of keys: blank, or zero */
  xb_index_info info;
  /* the cursor: an entry of a leaf, in the stored (ascending) order */
  int on;
  uint32_t leaf, at, recno;
  unsigned char key[CDX_KEY_MAX];
  uint32_t placed;           /* the bag's changes when the cursor got there */
};

struct xb_bag {
  xb_shared *s;
  xb_file f;
  int readonly, exclusive;
  uint32_t size;             /* the file's length */
  uint32_t end;              /* where pages are added: the file's length at the first one of the session, 0 before */
  uint32_t freeHead;         /* the free page list in memory (exclusive: it starts empty, as the reference's) */
  uint32_t writtenHead;      /* the head last written at offset 4 */
  uint32_t counter;
  xb_tag directory;
  xb_tag **tags;             /* in the order of their headers' offsets */
  uint32_t ntags;
  xb_tag **dead;             /* tags removed while orders may still name them: freed with the bag */
  uint32_t ndead;
  xb_pages pages;
  node *cache[CDX_CACHE];    /* each slot's page, by its number - 1: the first pages.count made, as pages came */
  size_t cacheBytes;         /* what the cache's nodes take, their key buffers with them */
  uint32_t changes;          /* changes of any of its tags: cursors placed before are placed again */
  uint32_t held;
  int heldOk;
  int wrote;                 /* shared: the file changed since the lock was taken */
  int stale;                 /* shared: the pages kept are read again at the next lock */
};

/* The bits of the largest number below 2^bits that holds `k`: the widths of a leaf's counts. */
static unsigned char bits_for(uint32_t k) {
  unsigned char b = 0;
  while ((1u << b) <= k) ++b;
  return b;
}

/* A leaf's parameters as a new leaf of the tag gets them. */
static void leaf_defaults(const xb_tag *t, node *p) {
  unsigned char b = bits_for(t->keyLen);
  p->db = p->tb = b;
  p->r = b <= 8 ? 3 : b <= 12 ? 4 : 5;
  p->rb = (unsigned char)(8 * p->r - 2 * b);
  p->recMask = p->rb >= 32 ? UINT32_MAX : (1u << p->rb) - 1;
  p->space = CDX_PAGE - CDX_LEAF_HEAD;
}

/* Room for `n` keys of the tag in the page's key buffer. */
static void keys_room(node *p, uint32_t n) {
  size_t len = p->tag->keyLen ? p->tag->keyLen : 1, need = (size_t)n * len;
  if (need <= p->cap) return;
  size_t cap = p->cap ? p->cap : 16 * len;
  while (cap < need) cap *= 2;
  unsigned char *k = realloc(p->keys, cap);
  if (!k) xb_gc_fatal("out of memory");
  if (p->self) p->tag->bag->cacheBytes += cap - p->cap; /* a node of the cache (not one being built) */
  p->keys = k;
  p->cap = cap;
}

static unsigned char *key_at(const node *p, uint32_t i) { return p->keys + (size_t)i * p->tag->keyLen; }

/* Decodes a page's header for tag `t` (its entries are left in its bytes): 0 when it is not a page of it. */
static int decode_head(node *p, const xb_tag *t) {
  const unsigned char *b = p->raw;
  p->tag = t;
  p->full = 0;
  p->kind = xb_get_le16(b);
  p->n = xb_get_le16(b + 2);
  p->left = xb_get_le32(b + 4);
  p->right = xb_get_le32(b + 8);
  if (p->n >= CDX_ENTRIES) return 0;
  if (!(p->kind & CDX_LEAF)) return p->n * (t->keyLen + CDX_BRANCH_TAIL) <= CDX_PAGE - CDX_BRANCH_HEAD;
  p->space = (int16_t)xb_get_le16(b + 12);
  p->recMask = xb_get_le32(b + 14);
  p->rb = b[20];
  p->db = b[21];
  p->tb = b[22];
  p->r = b[23];
  return p->r >= 3 && p->r <= 8 && (unsigned)(p->rb + p->db + p->tb) == 8u * p->r && p->db >= 1 && p->db <= 8 &&
         p->tb >= 1 && p->tb <= 8 && CDX_LEAF_HEAD + p->n * p->r <= CDX_PAGE;
}

/* Decodes the entries of a page whose header is: 0 when they are not entries of its tag. */
static int decode_entries(node *p) {
  const unsigned char *b = p->raw;
  const xb_tag *t = p->tag;
  uint32_t len = t->keyLen;
  keys_room(p, p->n + 1u);
  if (!(p->kind & CDX_LEAF)) {
    for (uint32_t i = 0; i < p->n; ++i) {
      const unsigned char *e = b + CDX_BRANCH_HEAD + (size_t)i * (len + CDX_BRANCH_TAIL);
      memcpy(key_at(p, i), e, len);
      p->rec[i] = xb_get_be32(e + len);
      p->child[i] = xb_get_be32(e + len + 4);
    }
    p->full = 1;
    return 1;
  }
  uint32_t end = CDX_PAGE, dupMask = (1u << p->db) - 1, trlMask = (1u << p->tb) - 1, trlShift = p->rb + p->db;
  uint64_t whole = p->r == 8 ? UINT64_MAX : ((uint64_t)1 << 8 * p->r) - 1;
  for (uint32_t i = 0; i < p->n; ++i) {
    uint64_t v = xb_get_le64(b + CDX_LEAF_HEAD + i * p->r) & whole; /* the buffer goes on past the page */
    uint32_t rec = (uint32_t)(v & p->recMask);
    uint32_t dup = (uint32_t)(v >> p->rb) & dupMask;
    uint32_t trl = (uint32_t)(v >> trlShift) & trlMask;
    if (dup > len || trl > len || dup + trl > len || (i == 0 && dup)) return 0;
    uint32_t mid = len - dup - trl;
    if (mid > end - CDX_LEAF_HEAD - p->n * p->r) return 0;
    end -= mid;
    unsigned char *k = key_at(p, i);
    if (dup) memcpy(k, key_at(p, i - 1), dup);
    memcpy(k + dup, b + end, mid);
    memset(k + dup + mid, t->fill, trl);
    p->rec[i] = rec;
    p->dup[i] = (unsigned char)dup;
    p->trl[i] = (unsigned char)trl;
  }
  p->full = 1;
  return 1;
}

/* Writes a page's header into its bytes (a leaf's parameters too). */
static void put_head(node *p) {
  unsigned char *b = p->raw;
  xb_put_le16(b, (uint16_t)p->kind);
  xb_put_le16(b + 2, (uint16_t)p->n);
  xb_put_le32(b + 4, p->left);
  xb_put_le32(b + 8, p->right);
  if (!(p->kind & CDX_LEAF)) return;
  xb_put_le16(b + 12, (uint16_t)p->space);
  xb_put_le32(b + 14, p->recMask);
  b[18] = (unsigned char)((1u << p->db) - 1);
  b[19] = (unsigned char)((1u << p->tb) - 1);
  b[20] = p->rb;
  b[21] = p->db;
  b[22] = p->tb;
  b[23] = p->r;
}

/* Writes a decoded page back into its bytes: a leaf wholly (the gap zeroed), a branch's entries over what was there
 * (bytes after the last stay as they were). A leaf over its room is not: it is filled anew before the operation
 * ends. */
static void encode(node *p) {
  unsigned char *b = p->raw;
  uint32_t len = p->tag->keyLen;
  p->dirty = 1;
  if (!p->full) { /* the entries are in the bytes: only the header changed */
    put_head(p);
    return;
  }
  if (!(p->kind & CDX_LEAF)) {
    put_head(p);
    for (uint32_t i = 0; i < p->n && CDX_BRANCH_HEAD + (i + 1u) * (len + CDX_BRANCH_TAIL) <= sizeof p->raw; ++i) {
      unsigned char *e = b + CDX_BRANCH_HEAD + (size_t)i * (len + CDX_BRANCH_TAIL);
      memcpy(e, key_at(p, i), len);
      xb_put_be32(e + len, p->rec[i]);
      xb_put_be32(e + len + 4, p->child[i]);
    }
    return;
  }
  if (p->space < 0) return;
  memset(b + CDX_LEAF_HEAD, 0, CDX_PAGE - CDX_LEAF_HEAD);
  put_head(p);
  uint32_t end = CDX_PAGE;
  for (uint32_t i = 0; i < p->n; ++i) {
    uint64_t v = (uint64_t)p->rec[i] | (uint64_t)p->dup[i] << p->rb | (uint64_t)p->trl[i] << (p->rb + p->db);
    for (uint32_t j = 0; j < p->r; ++j) b[CDX_LEAF_HEAD + i * p->r + j] = (unsigned char)(v >> 8 * j);
    uint32_t mid = len - p->dup[i] - p->trl[i];
    end -= mid;
    memcpy(b + end, key_at(p, i) + p->dup[i], mid);
  }
}

static node *kept(xb_bag *b, uint32_t offset) {
  uint16_t s = xb_pages_find(&b->pages, offset);
  return s ? b->cache[s - 1] : NULL;
}

/* Gives slot `p` page `offset` (0: none, the slot free); a page given is used now (an operation works on it). */
static void rehome(xb_bag *b, node *p, uint32_t offset) {
  p->offset = offset;
  xb_pages_place(&b->pages, p->self, offset);
  if (offset) xb_pages_use(&b->pages, p->self);
}

/* A page's bytes as its entries now are (a leaf's are made when they are needed). */
static void bytes_now(node *p) {
  if (!p->pending) return;
  p->pending = 0;
  encode(p);
}

/* The cache slot for page `offset`: the one holding it, else a free one, else a new one while the budget allows
 * (CDX_CACHE_MIN, CDX_CACHE_BYTES, CDX_CACHE), else the one used longest ago (written first when it changed; the pages
 * an operation works on are its latest, never taken). NULL after an error. */
static node *slot(xb_bag *b, uint32_t offset, int32_t *err) {
  node *victim = offset ? kept(b, offset) : NULL;
  if (victim) return victim;
  uint16_t s = xb_pages_spare(&b->pages, b->pages.count < CDX_CACHE_MIN || b->cacheBytes < CDX_CACHE_BYTES);
  if (s && !b->cache[s - 1]) {
    victim = calloc(1, sizeof *victim);
    if (!victim) xb_gc_fatal("out of memory");
    victim->self = s;
    b->cache[s - 1] = victim;
    b->cacheBytes += sizeof *victim;
    return victim;
  }
  victim = b->cache[(s ? s : xb_pages_oldest(&b->pages)) - 1];
  if (victim->offset && victim->dirty) {
    bytes_now(victim);
    if (!xb_platform_file_write_at(b->f, victim->offset, victim->raw, CDX_PAGE, err)) return NULL;
    victim->dirty = 0;
  }
  rehome(b, victim, 0);
  victim->pending = 0;
  victim->tag = NULL;
  return victim;
}

/* A page in the cache for tag `t`, read when it is not there, its header decoded (`whole`: its entries too). NULL
 * after an error (in `err`). */
static node *fetch(xb_bag *b, const xb_tag *t, uint32_t offset, int whole, int32_t *err) {
  if (!offset || offset % CDX_PAGE || offset >= b->size) {
    *err = XB_FERR_FAILURE;
    return NULL;
  }
  node *p = slot(b, offset, err);
  if (!p) return NULL;
  int ok = 1;
  if (p->offset == offset) {
    if (p->tag != t) { /* the same page seen through another tag: decoded again, or not one */
      bytes_now(p);
      ok = decode_head(p, t);
    }
  } else {
    if (!xb_file_read_at(b->f, offset, p->raw, CDX_PAGE, err)) return NULL;
    rehome(b, p, offset);
    p->dirty = 0;
    ok = decode_head(p, t);
  }
  ok = ok && (!whole || p->full || decode_entries(p));
  if (!ok) { /* not left half decoded */
    rehome(b, p, 0);
    p->tag = NULL;
    *err = XB_FERR_FAILURE;
    return NULL;
  }
  xb_pages_use(&b->pages, p->self);
  return p;
}

static node *get(xb_bag *b, const xb_tag *t, uint32_t offset, int32_t *err) { return fetch(b, t, offset, 1, err); }

static node *get_head(xb_bag *b, const xb_tag *t, uint32_t offset, int32_t *err) {
  return fetch(b, t, offset, 0, err);
}

/* The entries of a page fetched with its header only. 0 after an error. */
static int whole(node *p, int32_t *err) {
  if (p->full || decode_entries(p)) return 1;
  *err = XB_FERR_FAILURE;
  return 0;
}

/* The trailing fill bytes of a key, and how many bytes it repeats of `prev` (none: 0), at most its length less
 * those. */
static unsigned char trail_of(const xb_tag *t, const unsigned char *k) {
  uint32_t n = 0;
  while (n < t->keyLen && k[t->keyLen - 1 - n] == t->fill) ++n;
  return (unsigned char)n;
}

static unsigned char dup_of(const xb_tag *t, const unsigned char *prev, const unsigned char *k, uint32_t trl) {
  uint32_t lim = t->keyLen - trl, n = 0;
  while (n < lim && prev[n] == k[n]) ++n;
  return (unsigned char)n;
}

/* Keys in stored order: bytes, then record numbers. */
static int compare_entry(const xb_tag *t, const unsigned char *a, uint32_t ra, const unsigned char *b, uint32_t rb) {
  int c = memcmp(a, b, t->keyLen);
  if (c) return c;
  return ra < rb ? -1 : ra > rb;
}

/* Whether stored entry (k, rec) comes before bound `x` (in stored order). */
static int before(const xb_tag *t, const xb_index_bound *x, const unsigned char *k, uint32_t rec) {
  if (x->whole) return compare_entry(t, k, rec, x->key, x->recno) < 0;
  int c = memcmp(k, x->key, x->len);
  return x->past ? c <= 0 : c < 0;
}

/* A place among a tag's entries in stored order: entry `at` of leaf `leaf` (`at` == its count: past its last). */
typedef struct place {
  uint32_t leaf, at;
} place;

/* The pages from the root down to a leaf, and the entry followed in each (in the leaf: the place found). */
typedef struct trail {
  uint32_t offset[CDX_DEPTH], at[CDX_DEPTH];
  uint32_t depth;
} trail;

/* The way down to the first stored entry not before `x` (a branch's entries are the last of their children: past
 * them all, the last child). 0 after an error. */
static int descend(xb_tag *t, const xb_index_bound *x, trail *tr, int32_t *err) {
  uint32_t offset = t->root;
  for (uint32_t d = 0; d < CDX_DEPTH; ++d) {
    node *p = get(t->bag, t, offset, err);
    if (!p) return 0;
    uint32_t lo = 0, hi = p->n;
    while (lo < hi) {
      uint32_t mid = lo + (hi - lo) / 2;
      if (before(t, x, key_at(p, mid), p->rec[mid])) lo = mid + 1;
      else hi = mid;
    }
    tr->offset[d] = offset;
    if (p->kind & CDX_LEAF) {
      tr->at[d] = lo;
      tr->depth = d + 1;
      return 1;
    }
    if (!p->n) break;
    tr->at[d] = lo < p->n ? lo : p->n - 1u;
    offset = p->child[tr->at[d]];
  }
  *err = XB_FERR_FAILURE;
  return 0;
}

/* The place of the first stored entry not before `x`. 0 after an error. */
static int lower(xb_tag *t, const xb_index_bound *x, place *out, int32_t *err) {
  trail tr;
  if (!descend(t, x, &tr, err)) return 0;
  out->leaf = tr.offset[tr.depth - 1];
  out->at = tr.at[tr.depth - 1];
  return 1;
}

/* The first (`last`: the last) leaf of the tag. */
static int edge_leaf(xb_tag *t, int last, uint32_t *leaf, int32_t *err) {
  uint32_t offset = t->root;
  for (uint32_t depth = 0; depth < CDX_DEPTH; ++depth) {
    node *p = get(t->bag, t, offset, err);
    if (!p) return 0;
    if (p->kind & CDX_LEAF) {
      *leaf = offset;
      return 1;
    }
    if (!p->n) break;
    offset = p->child[last ? p->n - 1u : 0];
  }
  *err = XB_FERR_FAILURE;
  return 0;
}

/* Moves a place to a real entry: past a leaf's last onto the next leaf's first (forward), or before its first onto
 * the previous leaf's last (`back`, from `at` 0 meaning "before"). 1 on an entry, 0 past either end, -1 after an
 * error. */
static int settle(xb_tag *t, place *pl, int back, int32_t *err) {
  for (uint32_t guard = 0;; ++guard) {
    node *p = get_head(t->bag, t, pl->leaf, err);
    if (!p || guard > t->bag->size / CDX_PAGE) return -1;
    if (!back && pl->at < p->n) return 1;
    if (back && pl->at > 0 && pl->at <= p->n) {
      --pl->at;
      return 1;
    }
    uint32_t next = back ? p->left : p->right;
    if (next == CDX_NONE) return 0;
    node *q = get_head(t->bag, t, next, err);
    if (!q) return -1;
    pl->leaf = next;
    pl->at = back ? q->n : 0;
  }
}

/* Puts the cursor on a place's entry: its record number in `recno`. */
static int land(xb_tag *t, const place *pl, uint32_t *recno, int32_t *err) {
  node *p = get(t->bag, t, pl->leaf, err);
  if (!p) return -1;
  t->on = 1;
  t->leaf = pl->leaf;
  t->at = pl->at;
  t->recno = *recno = p->rec[pl->at];
  memcpy(t->key, key_at(p, pl->at), t->keyLen);
  t->placed = t->bag->changes;
  return 1;
}

/* The stored first (`last`: last) entry. */
static int go_edge(xb_tag *t, int last, uint32_t *recno, int32_t *err) {
  place pl;
  if (!edge_leaf(t, last, &pl.leaf, err)) return -1;
  node *p = get(t->bag, t, pl.leaf, err);
  if (!p) return -1;
  pl.at = last ? p->n : 0;
  int r = settle(t, &pl, last, err);
  if (r <= 0) t->on = 0;
  return r > 0 ? land(t, &pl, recno, err) : r;
}

/* The cursor's place now: found again by its entry when pages changed since it got there (`*gone`: the entry is
 * no more, and the place is where it would be). 0 after an error. */
static int cursor_place(xb_tag *t, place *pl, int *gone, int32_t *err) {
  *gone = 0;
  pl->leaf = t->leaf;
  pl->at = t->at;
  if (t->placed == t->bag->changes) return 1;
  xb_index_bound x = {t->key, t->keyLen, t->recno, 1, 0};
  if (!lower(t, &x, pl, err)) return 0;
  place here = *pl;
  int r = settle(t, &here, 0, err);
  if (r < 0) return 0;
  node *p = r ? get(t->bag, t, here.leaf, err) : NULL;
  if (r && !p) return 0;
  if (p && p->rec[here.at] == t->recno && memcmp(key_at(p, here.at), t->key, t->keyLen) == 0) *pl = here;
  else *gone = 1;
  return 1;
}

/* The stored entry after (`back`: before) the cursor's; when that entry went, the first after (before) where it
 * was. */
static int go_step(xb_tag *t, int back, uint32_t *recno, int32_t *err) {
  if (!t->on) return 0;
  place pl;
  int gone;
  if (!cursor_place(t, &pl, &gone, err)) return -1;
  if (!back && !gone) ++pl.at;
  int r = settle(t, &pl, back, err);
  if (r <= 0) t->on = 0;
  return r > 0 ? land(t, &pl, recno, err) : r;
}

/* The cursor on the first stored entry not before `x` (`back`: on the last one before it). */
static int go_bound(xb_tag *t, const xb_index_bound *x, int back, uint32_t *recno, int32_t *err) {
  place pl;
  if (!lower(t, x, &pl, err)) return -1;
  int r = settle(t, &pl, back, err);
  if (r <= 0) t->on = 0;
  return r > 0 ? land(t, &pl, recno, err) : r;
}

static int descending(const xb_tag *t) { return t->info.descend; }

/* SEEK: stored, the first entry whose prefix comes at or after the key (`last`: the last at or before it);
 * descending, the same the other way round. */
static int tag_seek(xb_tag *t, const unsigned char *key, size_t len, int last, uint32_t *recno, int32_t *err) {
  if (len > t->keyLen) len = t->keyLen;
  int back = descending(t) ? !last : last;
  xb_index_bound x = {key, len, 0, 0, back};
  return go_bound(t, &x, back, recno, err);
}

/* The entry (key, recno), or the first after where it would be (in the tag's order). */
static int tag_find(xb_tag *t, const unsigned char *key, uint32_t recno, uint32_t *found, int32_t *err) {
  xb_index_bound x = {key, t->keyLen, recno, 1, 0};
  place pl;
  if (!lower(t, &x, &pl, err)) return -1;
  place exact = pl;
  int r = settle(t, &exact, 0, err);
  if (r < 0) return -1;
  if (r > 0) {
    node *p = get(t->bag, t, exact.leaf, err);
    if (!p) return -1;
    if (p->rec[exact.at] == recno && memcmp(key_at(p, exact.at), key, t->keyLen) == 0)
      return land(t, &exact, found, err);
  }
  if (!descending(t)) {
    if (r <= 0) t->on = 0;
    return r > 0 ? land(t, &exact, found, err) : r;
  }
  r = settle(t, &pl, 1, err); /* descending: the one before where it would be */
  if (r <= 0) t->on = 0;
  return r > 0 ? land(t, &pl, found, err) : r;
}

/* The entries from place `a` up to place `b` (excluded), walking the leaves between: none when `b` comes before
 * `a` (bounds that cross). */
static int count_places(xb_tag *t, place a, place b, uint64_t *n, int32_t *err) {
  *n = 0;
  for (uint32_t guard = 0; a.leaf != b.leaf; ++guard) {
    node *p = get_head(t->bag, t, a.leaf, err);
    if (!p || guard > t->bag->size / CDX_PAGE) return 0;
    if (a.at < p->n) *n += p->n - a.at;
    if (p->right == CDX_NONE) { /* `b` is to the left */
      *n = 0;
      return 1;
    }
    a.leaf = p->right;
    a.at = 0;
  }
  if (b.at > a.at) *n += b.at - a.at;
  return 1;
}

/* The stored place of a bound of the tag's order (NULL: the start, `high`: the end): ascending, its first stored
 * entry at or after it; descending, the first stored entry the tag's order puts before it (the stored entries from
 * there on are the greater keys). */
static int bound_place(xb_tag *t, const xb_index_bound *x, int high, place *out, int32_t *err) {
  if (!x) { /* none: the end on that side */
    if (!edge_leaf(t, high, &out->leaf, err)) return 0;
    node *p = get(t->bag, t, out->leaf, err);
    if (!p) return 0;
    out->at = high ? p->n : 0;
    return 1;
  }
  if (!descending(t)) return lower(t, x, out, err);
  xb_index_bound y = *x; /* descending: before it the greater keys; at it what is stored up to it */
  if (y.whole) {
    if (y.recno == UINT32_MAX) return bound_place(t, NULL, 1, out, err);
    ++y.recno;
  } else {
    y.past = !y.past;
  }
  return lower(t, &y, out, err);
}

/* Stored, the range runs from lo's place to hi's; descending, from hi's to lo's (a missing bound: the end). */
static int tag_count(xb_tag *t, const xb_index_bound *lo, const xb_index_bound *hi, uint64_t *n, int32_t *err) {
  place a, b;
  int asc = !descending(t);
  if (!bound_place(t, asc ? lo : hi, 0, &a, err) || !bound_place(t, asc ? hi : lo, 1, &b, err)) return 0;
  return count_places(t, a, b, n, err);
}

static int tag_count_before(xb_tag *t, const xb_index_bound *lo, uint64_t *n, int32_t *err) {
  *n = 0;
  if (!t->on) return 1;
  place cur, from;
  int gone;
  if (!cursor_place(t, &cur, &gone, err)) return 0;
  if (!descending(t)) {
    if (!bound_place(t, lo, 0, &from, err)) return 0;
    return count_places(t, from, cur, n, err);
  }
  place to;
  if (!bound_place(t, lo, 1, &to, err)) return 0;
  if (!gone) ++cur.at; /* the entries stored after the cursor's, up to the bound */
  return count_places(t, cur, to, n, err);
}

static int tag_select(xb_tag *t, const xb_index_bound *lo, uint64_t k, uint32_t *recno, int32_t *err) {
  place pl;
  int back = descending(t);
  if (!bound_place(t, lo, back, &pl, err)) return -1;
  int r = settle(t, &pl, back, err);
  for (uint32_t guard = 0; r > 0 && k > 0; ++guard) { /* whole leaves at a time */
    node *p = get(t->bag, t, pl.leaf, err);
    if (!p || guard > t->bag->size / CDX_PAGE + 1) return -1;
    uint64_t here = back ? pl.at : p->n - 1u - pl.at; /* entries left in this leaf beyond the place */
    if (k <= here) {
      pl.at = back ? pl.at - (uint32_t)k : pl.at + (uint32_t)k;
      k = 0;
      break;
    }
    k -= here + 1;
    pl.at = back ? 0 : p->n; /* past this leaf's last (back: before its first) */
    r = settle(t, &pl, back, err);
  }
  if (r <= 0) {
    t->on = 0;
    return r;
  }
  return land(t, &pl, recno, err);
}

static const unsigned char *cdx_key(xb_tag *t) { return t->on ? t->key : NULL; }

static int cdx_at(const xb_tag *t, uint32_t recno) {
  return t->on && t->placed == t->bag->changes && t->recno == recno;
}

/* Keys. A number's: its double, big-endian, the sign bit set for one not below zero (minus zero is zero), every bit
 * turned for a negative one, so that bytes compare as the numbers do. */
static void put_number(double d, unsigned char *out) {
  if (d == 0) d = 0; /* minus zero */
  uint64_t bits;
  memcpy(&bits, &d, sizeof bits);
  for (int i = 0; i < 8; ++i) out[i] = (unsigned char)(bits >> 8 * (7 - i));
  if (out[0] & CDX_SIGN) {
    for (int i = 0; i < 8; ++i) out[i] = (unsigned char)~out[i];
  } else {
    out[0] ^= CDX_SIGN;
  }
}

static double get_number(const unsigned char *k) {
  unsigned char b[8];
  memcpy(b, k, 8);
  if (b[0] & CDX_SIGN) {
    b[0] ^= CDX_SIGN;
  } else {
    for (int i = 0; i < 8; ++i) b[i] = (unsigned char)~b[i];
  }
  uint64_t bits = 0;
  for (int i = 0; i < 8; ++i) bits = bits << 8 | b[i];
  double d;
  memcpy(&d, &bits, sizeof d);
  return d;
}

static void cdx_key_shape(const xb_item *v, char type, uint32_t *len, uint32_t *dec) {
  *dec = 0;
  if (type == 'C') *len = v->v.s->len > UINT32_MAX ? UINT32_MAX : (uint32_t)v->v.s->len;
  else *len = type == 'L' ? 1 : 8;
}

static int cdx_value_key(const xb_index_info *info, const xb_item *v, unsigned char *out, size_t *used) {
  uint32_t len = info->keyLen;
  *used = len;
  switch (info->type) {
  case 'C': {
    if (v->type != XB_T_STRING) return 0;
    size_t n = v->v.s->len < len ? v->v.s->len : len;
    memcpy(out, v->v.s->data, n);
    memset(out + n, ' ', len - n);
    *used = n;
    return 1;
  }
  case 'N':
    if (!xb_is_number(v)) return 0;
    put_number(xb_num_value(v), out);
    return 1;
  case 'D':
    if (v->type != XB_T_DATE) return 0;
    put_number((double)v->v.i, out);
    return 1;
  default:
    if (v->type != XB_T_LOGICAL) return 0;
    out[0] = v->v.l ? 'T' : 'F';
    return 1;
  }
}

static void cdx_key_value(const xb_index_info *info, const unsigned char *key, uint16_t dec, xb_item *r) {
  switch (info->type) {
  case 'C': xb_set_str(r, xb_str_new((const char *)key, info->keyLen)); return;
  case 'D': xb_set_date(r, (int64_t)get_number(key)); return;
  case 'L': xb_set_logical(r, key[0] == 'T'); return;
  default: xb_set_double(r, get_number(key), dec); return; /* the key keeps no decimals */
  }
}

/* The tag's order: bytes, the other way round for a descending tag. */
static int cdx_compare(const xb_index_info *info, const unsigned char *a, const unsigned char *b, size_t len) {
  int c = memcmp(a, b, len);
  return info->descend ? -c : c;
}

static int cdx_stored_descending(const xb_index_info *info) {
  (void)info;
  return 0; /* a descending tag is stored ascending */
}

/* A tag's expression from the header's text area: `len` bytes at `pos` (the length field less 1), up to a NUL,
 * trailing blanks left out. */
static void header_text(const unsigned char *h, uint32_t pos, uint32_t len, char *out) {
  uint32_t n = 0;
  while (n < len && pos + n < CDX_TEXT_MAX && n < XB_INDEX_KEY_MAX && h[CDX_TEXT + pos + n]) ++n;
  while (n && h[CDX_TEXT + pos + n - 1] == ' ') --n;
  memcpy(out, h + CDX_TEXT + pos, n);
  out[n] = '\0';
}

/* Reads a tag's header: 0 when it is not one. */
static int read_tag(xb_bag *b, xb_tag *t, uint32_t header, int32_t *err) {
  unsigned char h[CDX_HEADER];
  if (header % CDX_PAGE || header >= b->size || !xb_file_read_at(b->f, header, h, sizeof h, err)) {
    if (!*err) *err = XB_FERR_FAILURE;
    return 0;
  }
  memset(t, 0, sizeof *t);
  t->bag = b;
  t->header = header;
  t->root = xb_get_le32(h);
  t->keyLen = xb_get_le16(h + CDX_KEY_LEN);
  t->options = h[CDX_OPTIONS];
  if (!t->root || t->root % CDX_PAGE || t->root >= b->size || !t->keyLen || t->keyLen > CDX_KEY_MAX ||
      xb_get_le16(h + CDX_PAGE_LEN) != CDX_PAGE) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  uint32_t keyPos = xb_get_le16(h + CDX_KEY_POS), keyLen = xb_get_le16(h + CDX_KEY_LEN_TEXT);
  uint32_t forPos = xb_get_le16(h + CDX_FOR_POS), forLen = xb_get_le16(h + CDX_FOR_LEN);
  header_text(h, keyPos, keyLen ? keyLen - 1 : CDX_TEXT_MAX, t->info.key);
  if (forLen > 1) header_text(h, forPos, forLen - 1, t->info.cond);
  t->info.keyLen = t->keyLen;
  t->info.unique = (t->options & CDX_OPT_UNIQUE) != 0;
  t->info.descend = xb_get_le16(h + CDX_DESCEND) != 0;
  t->fill = ' ';
  return 1;
}

/* The tags the directory lists, in the order of their headers. */
static int read_directory(xb_bag *b, int32_t *err) {
  xb_tag *d = &b->directory;
  uint32_t leaf;
  if (!edge_leaf(d, 0, &leaf, err)) return 0;
  for (uint32_t guard = 0; leaf != CDX_NONE; ++guard) {
    node *p = get(b, d, leaf, err);
    if (!p || guard > b->size / CDX_PAGE) return 0;
    for (uint32_t i = 0; i < p->n; ++i) {
      xb_tag *t = calloc(1, sizeof *t);
      if (!t) xb_gc_fatal("out of memory");
      if (!read_tag(b, t, p->rec[i], err)) {
        free(t);
        return 0;
      }
      memcpy(t->name, key_at(p, i), CDX_NAME);
      uint32_t n = CDX_NAME;
      while (n && t->name[n - 1] == ' ') --n;
      t->name[n] = '\0';
      xb_tag **grown = realloc(b->tags, (b->ntags + 1) * sizeof *grown);
      if (!grown) xb_gc_fatal("out of memory");
      b->tags = grown;
      uint32_t at = b->ntags++;
      while (at > 0 && b->tags[at - 1]->header > t->header) { /* by header offset */
        b->tags[at] = b->tags[at - 1];
        --at;
      }
      b->tags[at] = t;
    }
    leaf = p->right;
  }
  return 1;
}

static void bag_free(xb_bag *b) {
  for (uint32_t i = 0; i < b->ntags; ++i) free(b->tags[i]);
  free(b->tags);
  for (uint32_t i = 0; i < b->ndead; ++i) free(b->dead[i]);
  free(b->dead);
  for (uint32_t i = 0; i < b->pages.count; ++i) {
    free(b->cache[i]->keys);
    free(b->cache[i]);
  }
  xb_pages_free(&b->pages);
  free(b);
}

static uint32_t cdx_tag_count(const xb_bag *b) { return b->ntags; }

static xb_tag *cdx_tag_at(xb_bag *b, uint32_t i, xb_index_info *info, const char **name) {
  xb_tag *t = b->tags[i];
  *info = t->info;
  *name = t->name;
  return t;
}

static void cdx_set_type(xb_tag *t, char type) {
  t->info.type = type;
  t->fill = type == 'C' ? ' ' : 0;
}

/* Shared files (docs/rdd.md): a byte of the file locked around every operation, shared to read and exclusive to
 * change it, and the 8 bytes at offset 4 read first: the free list's head and the change counter. Either differing
 * from the last seen, another program changed the file: the pages kept are dropped and the allocation area's end
 * found again. A change is written before the lock goes, the counter one higher. An exclusive file skips all of it
 * and writes the free list's head when it is flushed. */
static void forget(xb_bag *b) {
  for (uint32_t i = 0; i < b->pages.count; ++i) {
    node *p = b->cache[i];
    rehome(b, p, 0);
    p->dirty = 0;
    p->pending = 0;
    p->tag = NULL;
  }
}

static int unlock(xb_bag *b, int ok, int32_t *err) {
  int32_t unlockErr = 0;
  if (!xb_shared_unlock(b->s, XB_FOX_HIGH_LOCK, 1, &unlockErr) && ok) {
    *err = unlockErr;
    ok = 0;
  }
  return ok;
}

static int begin(xb_bag *b, int kind, int32_t *err) {
  if (b->exclusive || b->held) return 1;
  if (!xb_shared_lock(b->s, XB_FOX_HIGH_LOCK, 1, kind, 1, err)) return 0;
  b->wrote = 0;
  unsigned char h[8] = {0}; /* an empty file reads as zeros */
  if (xb_platform_file_read_at(b->f, CDX_FREE_HEAD, h, sizeof h, err) < 0) return unlock(b, 0, err);
  uint32_t head = xb_get_le32(h), counter = xb_get_be32(h + CDX_COUNTER - CDX_FREE_HEAD);
  if (!b->stale && head == b->freeHead && counter == b->counter) return 1;
  int64_t size = xb_platform_file_seek(b->f, 0, FS_END, err);
  if (size < 0) return unlock(b, 0, err);
  forget(b);
  b->stale = 0;
  b->freeHead = b->writtenHead = head;
  b->counter = counter;
  b->size = size > UINT32_MAX ? UINT32_MAX : (uint32_t)size;
  b->end = 0;
  ++b->changes;
  return 1;
}

static int write_pages(xb_bag *b, int32_t *err) {
  for (uint32_t i = 0; i < b->pages.count; ++i) {
    node *p = b->cache[i];
    if (p->offset && p->dirty) {
      bytes_now(p);
      if (!xb_platform_file_write_at(b->f, p->offset, p->raw, CDX_PAGE, err)) return 0;
      p->dirty = 0;
    }
  }
  return 1;
}

/* Exclusive: the free list's head written when it is not the one written last. */
static int write_head(xb_bag *b, int32_t *err) {
  if (b->freeHead == b->writtenHead) return 1;
  unsigned char word[4];
  xb_put_le32(word, b->freeHead);
  if (!xb_platform_file_write_at(b->f, CDX_FREE_HEAD, word, sizeof word, err)) return 0;
  b->writtenHead = b->freeHead;
  return 1;
}

/* Ends an operation that began (`ok`: it did not fail). */
static int finish(xb_bag *b, int ok, int32_t *err) {
  if (b->held) { /* the hold ends it */
    b->heldOk = b->heldOk && ok;
    return ok;
  }
  if (b->exclusive) return ok;
  if (b->wrote) {
    b->wrote = 0;
    if (ok) ok = write_pages(b, err);
    if (ok) {
      unsigned char h[8];
      xb_put_le32(h, b->freeHead);
      xb_put_be32(h + CDX_COUNTER - CDX_FREE_HEAD, b->counter + 1);
      ok = xb_platform_file_write_at(b->f, CDX_FREE_HEAD, h, sizeof h, err);
    }
    if (ok) {
      ++b->counter;
      b->writtenHead = b->freeHead;
    } else { /* nothing half done is written later: the file is read again at the next operation */
      forget(b);
      b->stale = 1;
    }
  }
  return unlock(b, ok, err);
}

static int cdx_hold(xb_bag *b, int write, int32_t *err) {
  if (b->held) {
    ++b->held;
    return 1;
  }
  if (!begin(b, write ? XB_LOCK_WRITE : XB_LOCK_READ, err)) return 0;
  b->held = 1;
  b->heldOk = 1;
  return 1;
}

static int cdx_let_go(xb_bag *b, int32_t *err) {
  if (--b->held) return 1;
  return finish(b, b->heldOk, err);
}

static int cdx_flush(xb_bag *b, int32_t *err) {
  if (b->readonly) return 1;
  return write_pages(b, err) && (!b->exclusive || write_head(b, err));
}

static int cdx_close(xb_bag *b, int32_t *err) {
  int ok = cdx_flush(b, err);
  int32_t closeErr = 0;
  if (!xb_shared_close(b->s, &closeErr) && ok) {
    *err = closeErr;
    ok = 0;
  }
  bag_free(b);
  return ok;
}

/* The cursor's operations, each under the file's read lock. */
static int cdx_first(xb_tag *t, uint32_t *recno, int32_t *err) {
  if (!begin(t->bag, XB_LOCK_READ, err)) return -1;
  int r = go_edge(t, descending(t), recno, err);
  return finish(t->bag, r >= 0, err) ? r : -1;
}

static int cdx_last(xb_tag *t, uint32_t *recno, int32_t *err) {
  if (!begin(t->bag, XB_LOCK_READ, err)) return -1;
  int r = go_edge(t, !descending(t), recno, err);
  return finish(t->bag, r >= 0, err) ? r : -1;
}

static int cdx_next(xb_tag *t, uint32_t *recno, int32_t *err) {
  if (!begin(t->bag, XB_LOCK_READ, err)) return -1;
  int r = go_step(t, descending(t), recno, err);
  return finish(t->bag, r >= 0, err) ? r : -1;
}

static int cdx_prev(xb_tag *t, uint32_t *recno, int32_t *err) {
  if (!begin(t->bag, XB_LOCK_READ, err)) return -1;
  int r = go_step(t, !descending(t), recno, err);
  return finish(t->bag, r >= 0, err) ? r : -1;
}

static int cdx_seek(xb_tag *t, const unsigned char *key, size_t len, int last, uint32_t *recno, int32_t *err) {
  if (!begin(t->bag, XB_LOCK_READ, err)) return -1;
  int r = tag_seek(t, key, len, last, recno, err);
  return finish(t->bag, r >= 0, err) ? r : -1;
}

static int cdx_find(xb_tag *t, const unsigned char *key, uint32_t recno, uint32_t *found, int32_t *err) {
  if (!begin(t->bag, XB_LOCK_READ, err)) return -1;
  int r = tag_find(t, key, recno, found, err);
  return finish(t->bag, r >= 0, err) ? r : -1;
}

static int cdx_count(xb_tag *t, const xb_index_bound *lo, const xb_index_bound *hi, uint64_t *n, int32_t *err) {
  if (!begin(t->bag, XB_LOCK_READ, err)) return 0;
  return finish(t->bag, tag_count(t, lo, hi, n, err), err);
}

static int cdx_count_before(xb_tag *t, const xb_index_bound *lo, uint64_t *n, int32_t *err) {
  if (!begin(t->bag, XB_LOCK_READ, err)) return 0;
  return finish(t->bag, tag_count_before(t, lo, n, err), err);
}

static int cdx_select(xb_tag *t, const xb_index_bound *lo, uint64_t k, uint32_t *recno, int32_t *err) {
  if (!begin(t->bag, XB_LOCK_READ, err)) return -1;
  int r = tag_select(t, lo, k, recno, err);
  return finish(t->bag, r >= 0, err) ? r : -1;
}

/* A page changed: its bytes again from its entries. */
static void touch(xb_bag *b, node *p) {
  if (p->kind & CDX_LEAF) { /* a leaf's bytes are made wholly from its entries: once, when they are needed */
    p->pending = 1;
    p->dirty = 1;
  } else { /* a branch's are written over its old ones: bytes past its last entry stay as the steps leave them */
    encode(p);
  }
  b->wrote = 1;
  ++b->changes;
}

/* Pages added and freed. A page comes from the free list (its head popped), else from the end of the allocation
 * area, which starts at the file's length; a tag header (two pages) always from the end, zeros written there at
 * once. A freed page holds the next free page's offset, then zeros, and becomes the head. */
static int grow(xb_bag *b, uint32_t by, uint32_t *offset, int32_t *err) {
  if (!b->end) b->end = b->size;
  *offset = b->end;
  if (*offset > UINT32_MAX - by) {
    *err = XB_FERR_DISK_FULL;
    return 0;
  }
  b->end = *offset + by;
  if (b->end > b->size) b->size = b->end;
  return 1;
}

/* Pops the free list's head: its successor, from the cache or the file, becomes the head. */
static int pop_free(xb_bag *b, uint32_t *offset, int32_t *err) {
  *offset = b->freeHead;
  if (*offset % CDX_PAGE || *offset >= b->size) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  const node *p = kept(b, *offset);
  if (p) {
    b->freeHead = xb_get_le32(p->raw);
    return 1;
  }
  unsigned char word[4];
  if (!xb_file_read_at(b->f, *offset, word, sizeof word, err)) return 0;
  b->freeHead = xb_get_le32(word);
  return 1;
}

static int take_page(xb_bag *b, uint32_t *offset, int32_t *err) {
  return b->freeHead ? pop_free(b, offset, err) : grow(b, CDX_PAGE, offset, err);
}

/* A new page of tag `t` (`leaf`: a leaf with the tag's parameters), empty, in the cache. NULL after an error. */
static node *new_page(xb_bag *b, const xb_tag *t, int leaf, int32_t *err) {
  uint32_t offset;
  if (!take_page(b, &offset, err)) return NULL;
  node *p = slot(b, offset, err);
  if (!p) return NULL;
  memset(p->raw, 0, sizeof p->raw);
  rehome(b, p, offset);
  p->tag = t;
  p->kind = leaf ? CDX_LEAF : 0;
  p->n = 0;
  p->full = 1;
  p->left = p->right = CDX_NONE;
  keys_room(p, 1);
  if (leaf) leaf_defaults(t, p);
  touch(b, p);
  return p;
}

static int free_page(xb_bag *b, uint32_t offset, int32_t *err) {
  node *p = slot(b, offset, err);
  if (!p) return 0;
  memset(p->raw, 0, sizeof p->raw);
  xb_put_le32(p->raw, b->freeHead);
  rehome(b, p, offset);
  p->pending = 0;
  p->tag = NULL;
  p->kind = 0;
  p->n = 0;
  p->full = 1;
  p->dirty = 1;
  b->freeHead = offset;
  b->wrote = 1;
  ++b->changes;
  return 1;
}

static int new_header(xb_bag *b, uint32_t *offset, int32_t *err) {
  static const unsigned char zeros[CDX_HEADER] = {0};
  if (!grow(b, CDX_HEADER, offset, err)) return 0;
  b->wrote = 1;
  return xb_platform_file_write_at(b->f, *offset, zeros, CDX_HEADER, err);
}

/* An expression's text as a header keeps it: without leading and trailing blanks. */
static size_t trimmed(const char *s, const char **from) {
  size_t n = strlen(s);
  while (n && *s == ' ') {
    ++s;
    --n;
  }
  while (n && s[n - 1] == ' ') --n;
  *from = s;
  return n;
}

/* Writes tag `t`'s header (the directory's, at offset 0, with the format's mark). */
static int write_header(xb_bag *b, const xb_tag *t, int32_t *err) {
  unsigned char h[CDX_HEADER] = {0};
  const char *key, *cond;
  size_t keyLen = trimmed(t->info.key, &key), condLen = trimmed(t->info.cond, &cond);
  if (keyLen + condLen > CDX_TEXT_ROOM) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  xb_put_le32(h, t->root);
  xb_put_le16(h + CDX_KEY_LEN, (uint16_t)t->keyLen);
  h[CDX_OPTIONS] = t->options;
  h[CDX_SIG] = CDX_SIGNATURE;
  xb_put_le16(h + CDX_HEADER_LEN, CDX_HEADER);
  xb_put_le16(h + CDX_PAGE_LEN, CDX_PAGE);
  if (!t->header) memcpy(h + CDX_MARK, cdx_mark, sizeof cdx_mark);
  xb_put_le16(h + CDX_DESCEND, (uint16_t)(t->info.descend != 0));
  memcpy(h + CDX_TEXT, key, keyLen);
  memcpy(h + CDX_TEXT + keyLen + 1, cond, condLen);
  xb_put_le16(h + CDX_FOR_POS, (uint16_t)(keyLen + 1));
  xb_put_le16(h + CDX_FOR_LEN, (uint16_t)(condLen + 1));
  xb_put_le16(h + CDX_KEY_LEN_TEXT, (uint16_t)(keyLen + 1));
  b->wrote = 1;
  return xb_platform_file_write_at(b->f, t->header, h, CDX_HEADER, err);
}

/* Widens a leaf by a byte of record number: every descriptor one byte longer. */
static void widen(node *p) {
  ++p->r;
  p->rb = (unsigned char)(p->rb + 8);
  p->recMask = p->recMask > UINT32_MAX >> 8 ? UINT32_MAX : p->recMask << 8 | 0xFF;
}

/* The most entries a branch of the tag holds. */
static uint32_t branch_max(const xb_tag *t) { return (CDX_PAGE - CDX_BRANCH_HEAD) / (t->keyLen + CDX_BRANCH_TAIL); }

/* The bulk build: the sorted entries added one by one, with one open page per level. An entry that does not fit
 * its level's page (a leaf: its free bytes; a branch: its most entries) starts a new page, taken before the parent
 * entry of the page it ends is added; the leaves of a table's tag are all as wide as its record count needs. Pages
 * are written as they are done, past the cache. */
typedef struct builder {
  xb_bag *b;
  xb_tag *t;
  uint32_t records;
  uint32_t levels;
  node *open[CDX_DEPTH];
  int32_t *err;
} builder;

static node *build_page(builder *bd, int leaf) {
  xb_bag *b = bd->b;
  uint32_t offset;
  if (!take_page(b, &offset, bd->err)) return NULL;
  node *old = kept(b, offset); /* a free page the cache kept: written from here */
  if (old) {
    rehome(b, old, 0);
    old->dirty = 0;
  }
  node *p = calloc(1, sizeof *p);
  if (!p) xb_gc_fatal("out of memory");
  p->offset = offset;
  p->tag = bd->t;
  p->full = 1;
  p->kind = leaf ? CDX_LEAF : 0;
  p->left = p->right = CDX_NONE;
  keys_room(p, CDX_ENTRIES);
  if (leaf) {
    leaf_defaults(bd->t, p);
    while (bd->records > p->recMask) widen(p);
  }
  return p;
}

static int build_write(builder *bd, node *p) {
  encode(p);
  int ok = xb_platform_file_write_at(bd->b->f, p->offset, p->raw, CDX_PAGE, bd->err);
  free(p->keys);
  free(p);
  return ok;
}

static int build_add(builder *bd, uint32_t level, const unsigned char *k, uint32_t rec, uint32_t child) {
  if (level >= CDX_DEPTH) {
    *bd->err = XB_FERR_FAILURE;
    return 0;
  }
  const xb_tag *t = bd->t;
  uint32_t len = t->keyLen;
  node *cur = level < bd->levels ? bd->open[level] : NULL;
  unsigned char trl = 0, dup = 0;
  int fresh;
  if (!level) {
    trl = trail_of(t, k);
    if (cur && cur->n) dup = dup_of(t, key_at(cur, cur->n - 1u), k, trl);
    fresh = !cur || cur->space - (int32_t)(len - dup - trl) - cur->r < 0;
  } else {
    fresh = !cur || cur->n >= branch_max(t);
  }
  if (fresh) {
    node *nw = build_page(bd, !level);
    if (!nw) return 0;
    dup = 0;
    bd->open[level] = nw;
    if (!cur) {
      bd->levels = level + 1;
    } else {
      cur->right = nw->offset;
      nw->left = cur->offset;
      int added = build_add(bd, level + 1, key_at(cur, cur->n - 1u), cur->rec[cur->n - 1u], cur->offset);
      if (!build_write(bd, cur) || !added) return 0;
    }
    cur = nw;
  }
  memcpy(key_at(cur, cur->n), k, len);
  cur->rec[cur->n] = rec;
  if (!level) {
    cur->dup[cur->n] = dup;
    cur->trl[cur->n] = trl;
    cur->space -= (int32_t)(cur->r + len - dup - trl);
  } else {
    cur->child[cur->n] = child;
  }
  ++cur->n;
  return 1;
}

/* Builds the tree of tag `t` from `count` sorted entries of a table of `records` records: its root in t->root. */
static int build_tree(xb_bag *b, xb_tag *t, const unsigned char *keys, const uint32_t *recs, uint32_t count,
                      uint32_t records, int32_t *err) {
  builder bd = {b, t, count ? records : 0, 0, {NULL}, err};
  int ok = 1;
  for (uint32_t i = 0; ok && i < count; ++i) ok = build_add(&bd, 0, keys + (size_t)i * t->keyLen, recs[i], 0);
  if (ok && !bd.levels) {
    ok = (bd.open[0] = build_page(&bd, 1)) != NULL;
    bd.levels = ok ? 1 : 0;
  }
  for (uint32_t level = 0; ok && level < bd.levels; ++level) {
    node *cur = bd.open[level];
    bd.open[level] = NULL;
    if (level + 1 == bd.levels) {
      cur->kind |= CDX_ROOT;
      t->root = cur->offset;
    } else {
      ok = build_add(&bd, level + 1, key_at(cur, cur->n - 1u), cur->rec[cur->n - 1u], cur->offset);
    }
    int written = build_write(&bd, cur);
    ok = ok && written;
  }
  for (uint32_t level = 0; level < bd.levels; ++level)
    if (bd.open[level]) {
      free(bd.open[level]->keys);
      free(bd.open[level]);
    }
  b->wrote = 1;
  ++b->changes;
  return ok;
}

/* Changes of a tree. A change at a leaf tells its parent what happened, and each level, up to the root, keeps its
 * entry of the child up to date and balances the child with its neighbours, telling its own parent in turn. */
#define F_LAST 1  /* the child's last entry changed */
#define F_OVER 2  /* a page holds more than it can */
#define F_EMPTY 4 /* the child has no entry left */
#define F_SPARE 8 /* there is room to spare: balancing may merge pages */

/* Inserts (key, rec) at `i` of a leaf: its flags. */
static int leaf_insert(node *p, uint32_t i, const unsigned char *k, uint32_t rec) {
  const xb_tag *t = p->tag;
  uint32_t len = t->keyLen;
  unsigned char oldDup = i < p->n ? p->dup[i] : 0, trl = trail_of(t, k), dup = 0;
  if (i > 0) {
    uint32_t lim = len - trl;
    if (oldDup > lim) {
      dup = (unsigned char)lim;
    } else {
      dup = oldDup;
      while (dup < lim && key_at(p, i - 1)[dup] == k[dup]) ++dup;
    }
  }
  int32_t space = (int32_t)(p->r + len - dup - trl);
  keys_room(p, p->n + 1u);
  memmove(key_at(p, i + 1), key_at(p, i), (size_t)(p->n - i) * len);
  memmove(&p->rec[i + 1], &p->rec[i], (p->n - i) * sizeof *p->rec);
  memmove(&p->dup[i + 1], &p->dup[i], p->n - i);
  memmove(&p->trl[i + 1], &p->trl[i], p->n - i);
  memcpy(key_at(p, i), k, len);
  p->rec[i] = rec;
  p->dup[i] = dup;
  p->trl[i] = trl;
  if (i < p->n) { /* the entry after it now follows the new key */
    unsigned char d = dup_of(t, k, key_at(p, i + 1), p->trl[i + 1]);
    space += (int32_t)p->dup[i + 1] - d;
    p->dup[i + 1] = d;
  }
  ++p->n;
  while (rec > p->recMask) {
    widen(p);
    space += (int32_t)p->n;
  }
  p->space -= space;
  int flags = 0;
  if (i + 1u >= p->n) flags |= F_LAST;
  if (p->space < 0) flags |= F_OVER;
  if (p->space >= p->r && p->left != CDX_NONE && p->right != CDX_NONE) flags |= F_SPARE;
  return flags;
}

/* Deletes entry `i` of a leaf (never narrowed): its flags. */
static int leaf_delete(node *p, uint32_t i) {
  uint32_t len = p->tag->keyLen;
  int32_t space = (int32_t)(p->r + len - p->dup[i] - p->trl[i]);
  if (i + 1u < p->n) {
    space -= p->dup[i + 1];
    unsigned char d = 0;
    if (i > 0) {
      uint32_t lim = len - p->trl[i + 1];
      d = p->dup[i + 1] < p->dup[i] ? p->dup[i + 1] : p->dup[i];
      if (d > lim) {
        d = (unsigned char)lim;
      } else {
        while (d < lim && key_at(p, i - 1)[d] == key_at(p, i + 1)[d]) ++d;
      }
    }
    space += d;
    p->dup[i + 1] = d;
  }
  p->space += space;
  memmove(key_at(p, i), key_at(p, i + 1), (size_t)(p->n - i - 1) * len);
  memmove(&p->rec[i], &p->rec[i + 1], (p->n - i - 1) * sizeof *p->rec);
  memmove(&p->dup[i], &p->dup[i + 1], p->n - i - 1u);
  memmove(&p->trl[i], &p->trl[i + 1], p->n - i - 1u);
  --p->n;
  int flags = i >= p->n ? F_LAST : 0;
  if (!p->n) flags |= F_EMPTY;
  else if (p->space < 0) flags |= F_OVER;
  if (p->space >= p->r) flags |= F_SPARE;
  return flags;
}

/* A branch's entry `i`: child page `c` with its last entry. */
static void branch_set(node *p, uint32_t i, const node *c) {
  memcpy(key_at(p, i), key_at(c, c->n - 1u), p->tag->keyLen);
  p->rec[i] = c->rec[c->n - 1u];
  p->child[i] = c->offset;
}

/* Opens room for an entry at `i` of a branch (set by the caller). */
static void branch_open(node *p, uint32_t i) {
  uint32_t len = p->tag->keyLen;
  keys_room(p, p->n + 1u);
  memmove(key_at(p, i + 1), key_at(p, i), (size_t)(p->n - i) * len);
  memmove(&p->rec[i + 1], &p->rec[i], (p->n - i) * sizeof *p->rec);
  memmove(&p->child[i + 1], &p->child[i], (p->n - i) * sizeof *p->child);
  ++p->n;
}

/* Removes entry `i` of a branch; the slot its last entry leaves is zeroed (others past the last stay as they are). */
static void branch_close(node *p, uint32_t i) {
  uint32_t len = p->tag->keyLen;
  memmove(key_at(p, i), key_at(p, i + 1), (size_t)(p->n - i - 1) * len);
  memmove(&p->rec[i], &p->rec[i + 1], (p->n - i - 1) * sizeof *p->rec);
  memmove(&p->child[i], &p->child[i + 1], (p->n - i - 1) * sizeof *p->child);
  --p->n;
  memset(p->raw + CDX_BRANCH_HEAD + (size_t)p->n * (len + CDX_BRANCH_TAIL), 0, len + CDX_BRANCH_TAIL);
}

/* Leaf entries a balancing moves between pages. */
typedef struct pool {
  unsigned char *keys;
  uint32_t *rec;
  unsigned char *dup, *trl;
  uint32_t n, cap, len;
} pool;

static unsigned char *pool_key(const pool *q, uint32_t i) { return q->keys + (size_t)i * q->len; }

static void pool_room(pool *q, uint32_t n) {
  if (n <= q->cap) return;
  uint32_t cap = q->cap ? q->cap : 256;
  while (cap < n) cap *= 2;
  unsigned char *keys = realloc(q->keys, (size_t)cap * q->len);
  if (keys) q->keys = keys;
  uint32_t *rec = realloc(q->rec, cap * sizeof *q->rec);
  if (rec) q->rec = rec;
  unsigned char *dup = realloc(q->dup, cap);
  if (dup) q->dup = dup;
  unsigned char *trl = realloc(q->trl, cap);
  if (trl) q->trl = trl;
  if (!keys || !rec || !dup || !trl) xb_gc_fatal("out of memory");
  q->cap = cap;
}

static void pool_free(pool *q) {
  free(q->keys);
  free(q->rec);
  free(q->dup);
  free(q->trl);
}

/* Appends a page's entries; the first one's repeated bytes counted again against the entry before it. */
static void pool_page(pool *q, const node *p) {
  uint32_t first = q->n;
  pool_room(q, q->n + p->n);
  memcpy(pool_key(q, first), p->keys, (size_t)p->n * q->len);
  memcpy(q->rec + first, p->rec, p->n * sizeof *q->rec);
  memcpy(q->dup + first, p->dup, p->n);
  memcpy(q->trl + first, p->trl, p->n);
  q->n += p->n;
  if (first && p->n) q->dup[first] = dup_of(p->tag, pool_key(q, first - 1), pool_key(q, first), q->trl[first]);
}

/* Drops the pool's first `n` entries. */
static void pool_drop(pool *q, uint32_t n) {
  memmove(q->keys, pool_key(q, n), (size_t)(q->n - n) * q->len);
  memmove(q->rec, q->rec + n, (q->n - n) * sizeof *q->rec);
  memmove(q->dup, q->dup + n, q->n - n);
  memmove(q->trl, q->trl + n, q->n - n);
  q->n -= n;
}

/* Fills leaf `p` anew from the pool's entries from `*at`: as many as fit, its parameters the defaults widened as
 * their record numbers need (a widening kept only when the entry then fits). */
static void leaf_fill(node *p, const pool *q, uint32_t *at) {
  uint32_t len = p->tag->keyLen;
  leaf_defaults(p->tag, p);
  p->n = 0;
  p->full = 1;
  for (; *at < q->n && p->n + 1u < CDX_ENTRIES; ++*at) {
    unsigned char dup = p->n ? q->dup[*at] : 0, trl = q->trl[*at], r = p->r;
    uint32_t mask = p->recMask, rec = q->rec[*at];
    int32_t size = (int32_t)(r + len - dup - trl);
    while (rec > mask) { /* a byte wider: one more for each descriptor, this one's included */
      ++r;
      mask = mask > UINT32_MAX >> 8 ? UINT32_MAX : mask << 8 | 0xFF;
      size += (int32_t)p->n + 1;
    }
    if (size > p->space) break;
    p->rb = (unsigned char)(p->rb + 8 * (r - p->r));
    p->r = r;
    p->recMask = mask;
    p->space -= size;
    keys_room(p, p->n + 1u);
    memcpy(key_at(p, p->n), pool_key(q, *at), len);
    p->rec[p->n] = rec;
    p->dup[p->n] = dup;
    p->trl[p->n] = trl;
    ++p->n;
  }
}

/* Whether the pool's entries from `at` fit in front of leaf `s`'s (its parameters widened for the largest record
 * number among them). */
static int fits_before(const node *s, const pool *q, uint32_t at) {
  uint32_t len = s->tag->keyLen, maxRec = 0;
  int32_t size = 0;
  for (uint32_t j = at; j < q->n; ++j) {
    if (q->rec[j] > maxRec) maxRec = q->rec[j];
    size += (int32_t)(len - (j == at ? 0u : q->dup[j]) - q->trl[j]);
  }
  if (s->n) size -= dup_of(s->tag, pool_key(q, q->n - 1u), key_at(s, 0), s->trl[0]);
  uint32_t r = s->r, mask = s->recMask;
  while (maxRec > mask) {
    ++r;
    mask = mask > UINT32_MAX >> 8 ? UINT32_MAX : mask << 8 | 0xFF;
  }
  size += (int32_t)((q->n - at) * r);
  return s->space - size - (int32_t)((r - s->r) * s->n) >= 0;
}

/* A page that is not what its place in the tree calls for: -1, the file damaged. */
static int damaged(int32_t *err) {
  *err = XB_FERR_FAILURE;
  return -1;
}

/* Balances the leaves under branch `p` around its child `c` that changed (its flags `in`): pages that overflow
 * spill into their neighbours or a new page; pages with a page's room to spare between them merge. The flags for
 * p's parent, or -1 after an error. */
static int balance_leaves(xb_bag *b, xb_tag *t, node *p, uint32_t c, int in, int32_t *err) {
  node *ch[CDX_WINDOW];
  uint32_t was[CDX_WINDOW];
  int32_t wasFree[CDX_WINDOW];
  uint32_t first, cnt = CDX_BALANCE;
  int up = 0;
  if (c > 0) {
    first = c - 1;
  } else {
    first = 0;
    --cnt;
    if (p->left != CDX_NONE) up |= F_SPARE;
  }
  if (cnt > p->n - first) {
    cnt = p->n - first;
    if (p->right != CDX_NONE) up |= F_SPARE;
  }
  if (!(in & (F_OVER | F_EMPTY)) && (cnt < 2 || !(in & F_SPARE))) return up;
  uint32_t skip = 0, keys = 0;
  int32_t spare = 0;
  for (uint32_t i = 0; i < cnt; ++i) {
    if (!(ch[i] = get_head(b, t, p->child[first + i], err))) return -1;
    if (!(ch[i]->kind & CDX_LEAF)) return damaged(err);
    was[i] = ch[i]->n;
    wasFree[i] = ch[i]->space;
    if (ch[i]->space >= ch[i]->r) {
      spare += ch[i]->space;
    } else if (ch[i]->space >= 0) { /* full */
      if (i == skip) ++skip;
      else if (i + 1u == cnt && !(in & F_OVER)) --cnt;
    }
    if (i >= skip && i < cnt) keys += ch[i]->n;
  }
  if (!(in & F_OVER))
    while (cnt - 1u > skip && ch[cnt - 1]->space >= 0 && ch[cnt - 1]->space < ch[cnt - 1]->r) keys -= ch[--cnt]->n;
  if (!(in & (F_OVER | F_EMPTY)) && (cnt < 2 || spare < CDX_PAGE - CDX_LEAF_HEAD)) return up;
  pool q = {NULL, NULL, NULL, NULL, 0, 0, t->keyLen};
  for (uint32_t i = skip; i < cnt && keys > 0; ++i) {
    if (!ch[i]->n) continue;
    if (!whole(ch[i], err)) {
      pool_free(&q);
      return -1;
    }
    uint32_t start = q.n;
    pool_page(&q, ch[i]);
    if (start && skip + 1u == i && ch[skip]->space >= 0 &&
        (int32_t)(t->keyLen - q.dup[start] - q.trl[start]) > ch[skip]->space - ch[skip]->r) {
      pool_drop(&q, start); /* the full page before cannot take even this page's first entry */
      keys -= ch[skip]->n;
      ++skip;
    }
  }
  uint32_t at = 0, j = skip;
  int fresh = 0, r = 0;
  while (!r && q.n > at) {
    if (j == cnt) {
      node *s = NULL;
      if (cnt >= CDX_WINDOW || (ch[j - 1]->right != CDX_NONE && !(s = get_head(b, t, ch[j - 1]->right, err)))) {
        r = cnt >= CDX_WINDOW ? damaged(err) : -1;
        break;
      }
      if (s && (!(s->kind & CDX_LEAF) || !whole(s, err))) {
        r = s->kind & CDX_LEAF ? -1 : damaged(err);
        break;
      }
      if (!fresh && s && fits_before(s, &q, at)) { /* the rest goes in front of the right neighbour's entries */
        pool_drop(&q, at);
        at = 0;
        pool_page(&q, s);
        ch[j] = s;
        if (first + j >= p->n) up |= F_LAST;
      } else {
        fresh = 1;
        node *nw = new_page(b, t, 1, err);
        if (!nw) {
          r = -1;
          break;
        }
        nw->left = ch[j - 1]->offset;
        nw->right = ch[j - 1]->right;
        ch[j - 1]->right = nw->offset;
        touch(b, ch[j - 1]);
        if (s) {
          s->left = nw->offset;
          touch(b, s);
        }
        ch[j] = nw;
        ++cnt;
        up |= F_SPARE;
      }
      was[j] = UINT32_MAX; /* never left as it was */
      wasFree[j] = 0;
    }
    leaf_fill(ch[j], &q, &at);
    if (!ch[j]->n) {
      r = damaged(err);
      break;
    }
    if (j == skip && j < cnt && first + j != c && ch[j]->n == was[j] && ch[j]->space == wasFree[j]) ++skip;
    else touch(b, ch[j]);
    if (j < cnt) {
      if (fresh) branch_open(p, first + j);
      branch_set(p, first + j, ch[j]);
    } else {
      ++cnt; /* the right neighbour: its last entry, and its parent's, stay */
    }
    ++j;
  }
  pool_free(&q);
  if (r < 0) return -1;
  if (j < cnt) { /* pages left empty leave the level */
    if (ch[cnt - 1]->right != CDX_NONE && (j > 1 || (j == 1 && ch[0]->left == CDX_NONE))) {
      /* the last kept page moves to the last emptied page's place, which its right neighbour points to */
      uint32_t moved = ch[cnt - 1]->offset, kept_at = ch[j - 1]->offset;
      rehome(b, ch[cnt - 1], 0);
      rehome(b, ch[j - 1], moved);
      rehome(b, ch[cnt - 1], kept_at);
      p->child[first + j - 1] = moved;
      ch[j - 1]->right = ch[cnt - 1]->right;
      touch(b, ch[j - 1]);
      if (j > 1) {
        ch[j - 2]->right = moved;
        touch(b, ch[j - 2]);
      }
    } else {
      uint32_t right = ch[cnt - 1]->right, left;
      if (j > 0) {
        left = ch[j - 1]->offset;
        ch[j - 1]->right = right;
        touch(b, ch[j - 1]);
      } else if ((left = ch[0]->left) != CDX_NONE) {
        node *l = get_head(b, t, left, err);
        if (!l) return -1;
        l->right = right;
        touch(b, l);
      }
      if (right != CDX_NONE) {
        node *rn = get_head(b, t, right, err);
        if (!rn) return -1;
        rn->left = left;
        touch(b, rn);
      }
    }
    while (j < cnt) {
      --cnt;
      branch_close(p, first + cnt);
      if (!free_page(b, ch[cnt]->offset, err)) return -1;
    }
    up |= F_SPARE;
  }
  touch(b, p);
  if (p->n > branch_max(t)) up |= F_OVER;
  return up;
}

/* Branch entries a balancing moves between pages. */
typedef struct branch_pool {
  unsigned char *keys;
  uint32_t *rec, *child;
  uint32_t n;
} branch_pool;

/* The share of `keys` entries over `pages` pages: as even as can be, a page holding at least two. */
static void share(uint32_t keys, uint32_t pages, uint32_t *lo, uint32_t *hi) {
  *lo = keys / pages > 2 ? keys / pages : 2;
  *hi = (keys + pages - 1) / pages > *lo ? (keys + pages - 1) / pages : *lo;
}

/* Balances the branches under branch `p` around its child `c` that changed: their entries spread evenly over as
 * many pages as they need. The flags for p's parent, or -1 after an error. */
static int balance_branches(xb_bag *b, xb_tag *t, node *p, uint32_t c, int in, int32_t *err) {
  int force = (in & (F_OVER | F_EMPTY)) != 0, up = 0;
  if (!force && !(in & F_SPARE)) return 0;
  uint32_t m = branch_max(t), len = t->keyLen, cnt = CDX_BALANCE, first;
  if (p->n <= cnt || c <= cnt / 2) first = 0;
  else if (c + cnt / 2 >= p->n) first = p->n - cnt;
  else first = c - cnt / 2;
  if (cnt > p->n - first) {
    cnt = p->n - first;
    up |= F_SPARE;
  }
  if (!force && cnt < 2) return up;
  node *ch[CDX_WINDOW];
  uint32_t keys = 0, most = 0, least = m;
  for (uint32_t i = 0; i < cnt; ++i) {
    if (!(ch[i] = get(b, t, p->child[first + i], err))) return -1;
    if (ch[i]->kind & CDX_LEAF) return damaged(err);
    keys += ch[i]->n;
    if (ch[i]->n > most) most = ch[i]->n;
    if (ch[i]->n < least) least = ch[i]->n;
  }
  uint32_t need = (keys + m - 1) / m;
  int32_t spread = (int32_t)most - (int32_t)least - (int32_t)(m / 2) + 1, extra = (int32_t)cnt - (int32_t)need;
  int32_t div = spread > extra ? spread : extra;
  if (keys > 0 && (div >= 2 || force)) {
    if (cnt == 1 && keys > m && ch[0]->right != CDX_NONE) { /* the right neighbour joins */
      if (!(ch[1] = get(b, t, ch[0]->right, err))) return -1;
      if (ch[1]->kind & CDX_LEAF) return damaged(err);
      keys += ch[1]->n;
      ++cnt;
      if (first + cnt > p->n) up |= F_LAST;
      need = (keys + m - 1) / m;
    } else { /* pages already holding their share stay out */
      uint32_t lo, hi;
      share(keys, need, &lo, &hi);
      while (cnt > 2 && ch[cnt - 1]->n >= lo && ch[cnt - 1]->n <= hi) {
        keys -= ch[--cnt]->n;
        share(keys, need, &lo, &hi);
      }
      while (cnt > 2 && ch[0]->n >= lo && ch[0]->n <= hi) {
        keys -= ch[0]->n;
        --cnt;
        ++first;
        memmove(ch, ch + 1, cnt * sizeof *ch);
        share(keys, need, &lo, &hi);
      }
    }
  }
  if (!force && (cnt < 2 || div < 2)) return up;
  branch_pool q = {NULL, NULL, NULL, 0};
  if (keys > 0) {
    q.keys = malloc((size_t)keys * len);
    q.rec = malloc(keys * sizeof *q.rec);
    q.child = malloc(keys * sizeof *q.child);
    if (!q.keys || !q.rec || !q.child) xb_gc_fatal("out of memory");
    for (uint32_t i = 0; i < cnt; ++i) {
      memcpy(q.keys + (size_t)q.n * len, ch[i]->keys, (size_t)ch[i]->n * len);
      memcpy(q.rec + q.n, ch[i]->rec, ch[i]->n * sizeof *q.rec);
      memcpy(q.child + q.n, ch[i]->child, ch[i]->n * sizeof *q.child);
      q.n += ch[i]->n;
    }
  }
  int ok = 1;
  if (need > cnt) { /* a page more, before the last */
    uint32_t i = cnt < 2 ? cnt : cnt - 1;
    node *nw = new_page(b, t, 0, err);
    ok = nw != NULL;
    if (ok) {
      if (i < cnt) ch[cnt] = ch[i];
      ch[i] = nw;
      branch_open(p, first + i); /* its entry is set with the shares */
      nw->left = ch[i - 1]->offset;
      nw->right = ch[i - 1]->right;
      ch[i - 1]->right = nw->offset;
      if (i < cnt) {
        ch[i + 1]->left = nw->offset;
      } else if (nw->right != CDX_NONE) {
        node *rn = get(b, t, nw->right, err);
        ok = rn != NULL;
        if (ok) {
          rn->left = nw->offset;
          touch(b, rn);
        }
      }
      ++cnt;
      up |= F_SPARE;
    }
  } else if (need < cnt) { /* pages less: the rightmost kept, as its right neighbour points to it */
    if (need > 1) {
      ch[need - 2]->right = ch[cnt - 1]->offset;
      ch[cnt - 1]->left = ch[need - 2]->offset;
      node *keep = ch[cnt - 1];
      ch[cnt - 1] = ch[need - 1];
      ch[need - 1] = keep;
    } else if (need > 0 && ch[0]->left == CDX_NONE) {
      node *keep = ch[cnt - 1];
      ch[cnt - 1] = ch[0];
      ch[0] = keep;
      ch[0]->left = CDX_NONE;
    } else {
      uint32_t right = ch[cnt - 1]->right, left;
      if (need > 0) {
        left = ch[need - 1]->offset;
        ch[need - 1]->right = right;
      } else if ((left = ch[0]->left) != CDX_NONE) {
        node *l = get(b, t, left, err);
        ok = l != NULL;
        if (ok) {
          l->right = right;
          touch(b, l);
        }
      }
      if (ok && right != CDX_NONE) {
        node *rn = get(b, t, right, err);
        ok = rn != NULL;
        if (ok) {
          rn->left = left;
          touch(b, rn);
        }
      }
    }
    for (uint32_t i = cnt; ok && i-- > need;) {
      branch_close(p, first + i);
      ok = free_page(b, ch[i]->offset, err);
    }
    cnt = need;
    up |= F_SPARE;
  }
  if (ok && keys > 0) { /* even shares, in order (the stale bytes past a page's last entry stay) */
    int odd = m == 2 && cnt > 2 && keys == 2 * cnt - 1;
    uint32_t from = 0;
    for (uint32_t i = 0; i < cnt; ++i) {
      uint32_t take = odd && i == 1 ? 1 : (keys + cnt - i - 1) / (cnt - i);
      node *k = ch[i];
      keys_room(k, take);
      memcpy(k->keys, q.keys + (size_t)from * len, (size_t)take * len);
      memcpy(k->rec, q.rec + from, take * sizeof *k->rec);
      memcpy(k->child, q.child + from, take * sizeof *k->child);
      k->n = take;
      touch(b, k);
      from += take;
      keys -= take;
      if (first + i < p->n) branch_set(p, first + i, k);
    }
  }
  free(q.keys);
  free(q.rec);
  free(q.child);
  if (!ok) return -1;
  touch(b, p);
  if (p->n > m) up |= F_OVER;
  return up;
}

static int fix_root(xb_bag *b, xb_tag *t, node *root, int flags, int32_t *err);

/* A full root: its entries move to a new page, and the root, at the same place (the tag's header never changes),
 * becomes a branch over it, then balanced. */
static int root_split(xb_bag *b, xb_tag *t, node *root, int32_t *err) {
  node *top = new_page(b, t, 0, err);
  if (!top) return 0;
  uint32_t moved = top->offset, at = root->offset;
  rehome(b, top, 0);
  rehome(b, root, moved);
  rehome(b, top, at);
  root->kind &= ~(uint32_t)CDX_ROOT;
  touch(b, root);
  top->kind = CDX_ROOT;
  branch_open(top, 0);
  branch_set(top, 0, root);
  touch(b, top);
  int r = root->kind & CDX_LEAF ? balance_leaves(b, t, top, 0, F_OVER, err)
                                : balance_branches(b, t, top, 0, F_OVER, err);
  return r >= 0 && fix_root(b, t, top, r, err);
}

/* The root after a change: one left without entries becomes an empty leaf (a former branch's bytes past the leaf
 * header stay), one that overflows splits. */
static int fix_root(xb_bag *b, xb_tag *t, node *root, int flags, int32_t *err) {
  if (root->n) return flags & F_OVER ? root_split(b, t, root, err) : 1;
  int wasLeaf = (root->kind & CDX_LEAF) != 0;
  root->kind |= CDX_LEAF;
  leaf_defaults(t, root);
  if (wasLeaf) {
    touch(b, root);
  } else {
    put_head(root);
    root->dirty = 1;
    b->wrote = 1;
    ++b->changes;
  }
  return 1;
}

/* After a change of the leaf at the end of `tr` (its flags): each level up to the root sets its entry of the child
 * to the child's last entry, then balances the child with its neighbours. */
static int fix_up(xb_bag *b, xb_tag *t, const trail *tr, int flags, int32_t *err) {
  for (uint32_t d = tr->depth - 1; d-- > 0;) {
    node *p = get(b, t, tr->offset[d], err), *child = p ? get(b, t, tr->offset[d + 1], err) : NULL;
    if (!child) return 0;
    uint32_t c = tr->at[d];
    int up = 0;
    if (flags & F_LAST) {
      if (!child->n) {
        flags |= F_EMPTY;
        up |= F_LAST;
      } else {
        branch_set(p, c, child);
        touch(b, p);
        if (c + 1u >= p->n) up |= F_LAST;
      }
    }
    int r = child->kind & CDX_LEAF ? balance_leaves(b, t, p, c, flags, err)
                                   : balance_branches(b, t, p, c, flags, err);
    if (r < 0) return 0;
    flags = up | r;
  }
  node *root = get(b, t, tr->offset[0], err);
  return root && fix_root(b, t, root, flags, err);
}

/* Adds entry (key, rec): nothing when it is there (a unique tag: when its key is). */
static int tree_add(xb_bag *b, xb_tag *t, const unsigned char *k, uint32_t rec, int32_t *err) {
  xb_index_bound x = {k, t->keyLen, rec, !t->info.unique, 0};
  trail tr;
  if (!descend(t, &x, &tr, err)) return 0;
  node *leaf = get(b, t, tr.offset[tr.depth - 1], err);
  if (!leaf) return 0;
  uint32_t i = tr.at[tr.depth - 1];
  if (i < leaf->n && memcmp(key_at(leaf, i), k, t->keyLen) == 0 && (t->info.unique || leaf->rec[i] == rec)) return 1;
  int flags = leaf_insert(leaf, i, k, rec);
  touch(b, leaf);
  return fix_up(b, t, &tr, flags, err);
}

/* Removes entry (key, rec): nothing when it is not there. */
static int tree_remove(xb_bag *b, xb_tag *t, const unsigned char *k, uint32_t rec, int32_t *err) {
  xb_index_bound x = {k, t->keyLen, rec, 1, 0};
  trail tr;
  if (!descend(t, &x, &tr, err)) return 0;
  node *leaf = get(b, t, tr.offset[tr.depth - 1], err);
  if (!leaf) return 0;
  uint32_t i = tr.at[tr.depth - 1];
  if (i >= leaf->n || leaf->rec[i] != rec || memcmp(key_at(leaf, i), k, t->keyLen) != 0) return 1;
  int flags = leaf_delete(leaf, i);
  touch(b, leaf);
  return fix_up(b, t, &tr, flags, err);
}

static int cdx_replace(xb_tag *t, const unsigned char *old, const unsigned char *key, uint32_t recno, int32_t *err) {
  xb_bag *b = t->bag;
  if (!begin(b, XB_LOCK_WRITE, err)) return 0;
  int ok = (!old || tree_remove(b, t, old, recno, err)) && (!key || tree_add(b, t, key, recno, err));
  t->on = 0;
  return finish(b, ok, err);
}

/* The directory's key of a tag: its name padded with blanks. */
static void name_key(const xb_tag *t, unsigned char *k) {
  size_t n = strlen(t->name);
  memcpy(k, t->name, n);
  memset(k + n, ' ', CDX_NAME - n);
}

/* Frees a tag's pages, each child's before its branch, the root last. */
static int free_tree(xb_bag *b, xb_tag *t, uint32_t offset, uint32_t depth, int32_t *err) {
  if (depth >= CDX_DEPTH) return damaged(err) + 1;
  node *p = get(b, t, offset, err);
  if (!p) return 0;
  if (!(p->kind & CDX_LEAF)) {
    uint32_t n = p->n, kids[CDX_ENTRIES];
    memcpy(kids, p->child, n * sizeof *kids);
    for (uint32_t i = 0; i < n; ++i)
      if (!free_tree(b, t, kids[i], depth + 1, err)) return 0;
  }
  return free_page(b, offset, err);
}

/* Keeps a tag the bag no longer lists until the bag goes: an order may still name it, but finds no page through it
 * (its pages may be another tag's now). */
static void retire(xb_bag *b, xb_tag *t) {
  xb_tag **grown = realloc(b->dead, (b->ndead + 1) * sizeof *grown);
  if (!grown) xb_gc_fatal("out of memory");
  b->dead = grown;
  b->dead[b->ndead++] = t;
  t->on = 0;
  t->root = 0;
}

/* Takes tag `t` out of the bag: out of the directory and, when other tags stay in an exclusive file, its pages and
 * header freed (a shared file leaves them). */
static int remove_tag(xb_bag *b, xb_tag *t, int32_t *err) {
  uint32_t i = 0;
  while (i < b->ntags && b->tags[i] != t) ++i;
  if (i == b->ntags) { /* retired already */
    *err = XB_FERR_FAILURE;
    return 0;
  }
  unsigned char k[CDX_NAME];
  name_key(t, k);
  int ok = tree_remove(b, &b->directory, k, t->header, err);
  if (ok && b->ntags > 1 && b->exclusive)
    ok = free_tree(b, t, t->root, 0, err) && free_page(b, t->header, err) &&
         free_page(b, t->header + CDX_PAGE, err);
  memmove(b->tags + i, b->tags + i + 1, (b->ntags - i - 1) * sizeof *b->tags);
  --b->ntags;
  retire(b, t);
  ++b->changes;
  return ok;
}

static int cdx_remove(xb_bag *b, xb_tag *t, int32_t *err) {
  if (!begin(b, XB_LOCK_WRITE, err)) return 0;
  return finish(b, remove_tag(b, t, err), err);
}

/* Empties the file and starts it again with the directory alone: its header at 0, its root an empty leaf after it.
 * The tags it had are retired. */
static int start_file(xb_bag *b, int32_t *err) {
  while (b->ntags) retire(b, b->tags[--b->ntags]);
  forget(b);
  if (!xb_platform_file_truncate(b->f, 0, err)) return 0;
  b->size = b->end = 0;
  b->freeHead = b->writtenHead = 0;
  b->wrote = 1;
  ++b->changes;
  xb_tag *d = &b->directory;
  memset(d, 0, sizeof *d);
  d->bag = b;
  d->keyLen = CDX_NAME;
  d->options = CDX_OPT_DIRECTORY | CDX_OPT_COMPOUND | CDX_OPT_COMPACT;
  d->fill = ' ';
  d->info.type = 'C';
  d->info.keyLen = CDX_NAME;
  uint32_t header;
  if (!new_header(b, &header, err)) return 0;
  node *root = new_page(b, d, 1, err);
  if (!root) return 0;
  root->kind |= CDX_ROOT;
  touch(b, root);
  d->root = root->offset;
  return write_header(b, d, err);
}

static xb_tag *find_tag(const xb_bag *b, const char *name) {
  for (uint32_t i = 0; i < b->ntags; ++i)
    if (strcmp(b->tags[i]->name, name) == 0) return b->tags[i];
  return NULL;
}

/* Adds tag `name` from the sorted entries: its header at the end of the file, its pages, then its directory entry. */
static int add_tag(xb_bag *b, const char *name, const xb_index_info *info, const unsigned char *keys,
                   const uint32_t *recnos, uint32_t count, uint32_t records, xb_tag **out, int32_t *err) {
  xb_tag *t = calloc(1, sizeof *t);
  if (!t) xb_gc_fatal("out of memory");
  t->bag = b;
  strcpy(t->name, name);
  t->keyLen = info->keyLen;
  t->info = *info;
  t->options = (unsigned char)(CDX_OPT_COMPOUND | CDX_OPT_COMPACT | (info->unique ? CDX_OPT_UNIQUE : 0) |
                               (info->cond[0] ? CDX_OPT_FOR : 0) | (info->partial ? CDX_OPT_PARTIAL : 0));
  cdx_set_type(t, info->type);
  unsigned char k[CDX_NAME];
  name_key(t, k);
  int ok = new_header(b, &t->header, err) && build_tree(b, t, keys, recnos, count, records, err) &&
           tree_add(b, &b->directory, k, t->header, err) && write_header(b, t, err);
  if (!ok) {
    free(t);
    return 0;
  }
  xb_tag **grown = realloc(b->tags, (b->ntags + 1) * sizeof *grown);
  if (!grown) xb_gc_fatal("out of memory");
  b->tags = grown;
  b->tags[b->ntags++] = t; /* its header is the file's last */
  *out = t;
  return 1;
}

static xb_bag *bag_new(xb_shared *s, int readonly, int exclusive) {
  xb_bag *b = calloc(1, sizeof *b);
  if (!b) xb_gc_fatal("out of memory");
  xb_pages_init(&b->pages, CDX_CACHE);
  b->s = s;
  b->f = xb_shared_handle(s);
  b->readonly = readonly;
  b->exclusive = exclusive;
  b->stale = !exclusive;
  return b;
}

/* Reads the directory and the tags it lists: 0 when the file is not a CDX file. */
static int load(xb_bag *b, int32_t *err) {
  int64_t size = xb_platform_file_seek(b->f, 0, FS_END, err);
  if (size < 0) return 0;
  b->size = size > UINT32_MAX ? UINT32_MAX : (uint32_t)size;
  if (size <= CDX_HEADER || size > UINT32_MAX) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  if (!read_tag(b, &b->directory, 0, err)) return 0;
  b->directory.fill = ' ';
  b->directory.info.type = 'C';
  if (b->directory.keyLen != CDX_NAME) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  return read_directory(b, err);
}

static int cdx_open(const char *path, int readonly, int exclusive, xb_bag **out, int32_t *err) {
  xb_shared *s;
  if (!xb_shared_open(path, readonly, exclusive, 0, &s, err)) return 0;
  xb_bag *b = bag_new(s, readonly, exclusive);
  int ok = begin(b, XB_LOCK_READ, err);
  if (ok) {
    ok = load(b, err);
    int ended = finish(b, ok, err);
    ok = ok && ended;
  }
  if (!ok) {
    if (!*err) *err = XB_FERR_FAILURE;
    int32_t ignored;
    xb_shared_close(b->s, &ignored);
    bag_free(b);
    return 0;
  }
  *out = b;
  return 1;
}

/* Tag `name` from the sorted entries, in the bag open in `*bag` or in the file at `path`: added, or replacing the
 * tag of that name. The file is made anew when `fresh`, when it holds no CDX file, and when its only tag is
 * replaced. A file made here is made exclusive, then opened as asked. */
static int cdx_build(xb_bag **bag, const char *path, const char *name, int fresh, const xb_index_info *info,
                     const unsigned char *keys, const uint32_t *recnos, uint32_t count, uint32_t records, int readonly,
                     int exclusive, xb_tag **tag, int32_t *err) {
  if (strlen(name) > CDX_NAME) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  if (readonly || (*bag && (*bag)->readonly)) {
    *err = XB_FERR_ACCESS;
    return 0;
  }
  xb_bag *b = *bag;
  int made = 0, ok = 1;
  if (!b) {
    xb_file_id id;
    int32_t ignored;
    made = fresh || !xb_platform_path_id(path, &id, &ignored);
    xb_shared *s;
    if (!xb_shared_open(path, 0, made || exclusive, made, &s, err)) return 0;
    b = bag_new(s, 0, exclusive);
  }
  if (made) { /* nobody else has the file: no lock to take */
    b->held = 1;
    b->heldOk = 1;
    fresh = 1;
  } else {
    ok = cdx_hold(b, 1, err);
  }
  if (ok && !*bag && !made && !load(b, err)) { /* an empty file is made anew; another one is no CDX file to lose */
    for (uint32_t i = 0; i < b->ntags; ++i) free(b->tags[i]);
    b->ntags = 0;
    if (b->size) {
      b->heldOk = 0;
      ok = 0;
    } else {
      *err = 0;
      fresh = 1;
    }
  }
  xb_tag *t = NULL;
  if (b->held) {
    xb_tag *old = !ok || fresh ? NULL : find_tag(b, name);
    if (ok && old) ok = remove_tag(b, old, err);
    if (ok && (fresh || !b->ntags)) ok = start_file(b, err);
    if (ok) ok = add_tag(b, name, info, keys, recnos, count, records, &t, err);
    if (!ok) b->heldOk = 0;
    int released = cdx_let_go(b, err);
    ok = ok && released;
  }
  if (!*bag) {
    int32_t ignored;
    if (!ok) { /* what a failed build left half done is not written */
      forget(b);
      cdx_close(b, &ignored);
      return 0;
    }
    if (made && !exclusive) { /* made exclusive: opened again shared */
      if (!cdx_close(b, err) || !cdx_open(path, 0, 0, &b, err)) return 0;
      if (!(t = find_tag(b, name))) {
        cdx_close(b, &ignored);
        *err = XB_FERR_FAILURE;
        return 0;
      }
      cdx_set_type(t, info->type);
    }
    *bag = b;
  }
  if (!ok) return 0;
  *tag = t;
  return 1;
}

const xb_index_format xb_index_cdx = {
    .ext = ".cdx",
    .tagged = 1,
    .keyMax = CDX_KEY_MAX,
    .tiesByRecord = 1,
    .key_shape = cdx_key_shape,
    .value_key = cdx_value_key,
    .key_value = cdx_key_value,
    .compare = cdx_compare,
    .stored_descending = cdx_stored_descending,
    .open = cdx_open,
    .tag_count = cdx_tag_count,
    .tag_at = cdx_tag_at,
    .build = cdx_build,
    .remove = cdx_remove,
    .set_type = cdx_set_type,
    .hold = cdx_hold,
    .let_go = cdx_let_go,
    .flush = cdx_flush,
    .close = cdx_close,
    .first = cdx_first,
    .last = cdx_last,
    .next = cdx_next,
    .prev = cdx_prev,
    .seek = cdx_seek,
    .find = cdx_find,
    .count = cdx_count,
    .count_before = cdx_count_before,
    .select = cdx_select,
    .key = cdx_key,
    .at = cdx_at,
    .replace = cdx_replace,
};
