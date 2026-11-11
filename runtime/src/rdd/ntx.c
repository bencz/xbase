/* NTX index files (rdd/ntx.h). Every binary field is little-endian (core/bytes.h):
 *
 *   page 0, the header: 0-1 the signature (6; 7 with a FOR condition), 2-3 a counter of changes, 4-7 the root page's
 *     offset, 8-11 the first free page's (0: none), 12-13 an entry's size (the key's length and 8), 14-15 the key's
 *     length, 16-17 its decimals, 18-19 the entries a page holds at most, 20-21 half of that, 22-277 the key
 *     expression's text, 278 1 for a unique order, 280 1 for a descending one, 282-537 the FOR condition's text;
 *     zeros elsewhere;
 *   every other page: 0-1 its entry count n, then max + 1 offsets (2 bytes) in the page of its entries in order, then
 *     of its free slots. An entry is the offset of the page of the keys before it (0 in a leaf), the record number and
 *     the key; the one after the last (at offset n) holds only the page of the keys after all the others;
 *   a free page has count 0, and in its only entry's page offset the next free page (0 ends the list).
 *
 * A page holds at most floor(1022 / (entry size + 2)) - 1 entries, made even; the tree keeps every page but the root
 * at half of that or more. */
#include "rdd/ntx.h"

#include "rdd/rdd.h"

#include "core/bytes.h"
#include "core/file.h"
#include "core/shared_file.h"

#include <stdlib.h>
#include <string.h>

#define NTX_PAGE 1024
#define NTX_SIG 6
#define NTX_SIG_COND 7
#define NTX_VERSION 2      /* the header's fields: their offsets */
#define NTX_ROOT 4
#define NTX_FREE 8
#define NTX_ENTRY_SIZE 12
#define NTX_KEY_LEN 14
#define NTX_KEY_DEC 16
#define NTX_MAX 18
#define NTX_HALF 20
#define NTX_COUNTS 12      /* the header's bytes a flush rewrites: signature, counter, root, free list */
#define NTX_KEY_TEXT 22
#define NTX_UNIQUE 278
#define NTX_DESCEND 280
#define NTX_COND_TEXT 282
#define NTX_ENTRY_HEAD 8 /* the page offset and the record number before the key */
#define NTX_CACHE 128    /* pages kept in memory per file */
#define NTX_BUCKET_BITS 8 /* the cache's hash table: page number to slot */
#define NTX_BUCKETS (1u << NTX_BUCKET_BITS)
#define NTX_NONE 0xFFFF
#define NTX_DEPTH 40     /* levels a tree may have: past what 4 GB of pages holds */
#define NTX_CHILDREN (NTX_PAGE / NTX_ENTRY_HEAD + 1) /* more than a page's entries and the page after them */

typedef struct page {
  uint32_t offset; /* in the file; 0: a slot with no page */
  uint16_t next;   /* the next slot of its hash bucket, NTX_NONE at the end */
  uint32_t used;   /* the clock of its last use */
  uint32_t pins;   /* uses in progress: never evicted then */
  int dirty;
  unsigned char b[NTX_PAGE];
} page;

struct xb_ntx {
  xb_shared *s;
  xb_file f;                          /* s's handle */
  int readonly;
  int shared;                         /* each operation under the lock, the header read again (docs/rdd.md) */
  xb_ntx_info info;
  uint32_t entrySize, max, half, base; /* base: where the entries start in a page */
  uint32_t root, freeList, end;       /* end: the offset of a page added at the end of the file */
  uint16_t version;
  int headerDirty;
  int changed;                        /* since the last flush: the counter goes up once per flush */
  page *cache;
  uint16_t buckets[NTX_BUCKETS];      /* the first slot of each hash bucket, NTX_NONE for none */
  uint32_t clock, hint;               /* hint: the slot used last */
  unsigned char *scratch;             /* 2 * max + 4 entries, for splits, shifts and merges */
  /* The cursor: path[0] is the root; the last level holds the page and index of the entry, the others the page and
   * the index of the child descended. */
  struct {
    uint32_t page, pos;
  } path[NTX_DEPTH];
  uint32_t depth;
  int on;
  uint32_t onRecno;
  unsigned char onKey[XB_NTX_KEY_MAX]; /* shared: the key of the entry the cursor is on */
  int displaced;                      /* shared: that entry went; the cursor is on the one after it (or none) */
  int stale;                          /* shared: a change failed half way: the pages kept are read again */
  uint32_t changes, placed; /* changes made, and their number when the cursor was placed */
  uint32_t visits;          /* pages a count may still read: a broken file's loop ends as an error */
};

/* An entry of a page buffer. */
static uint32_t count(const unsigned char *b) { return xb_get_le16(b); }
static unsigned char *entry(unsigned char *b, uint32_t i) { return b + xb_get_le16(b + 2 + 2 * i); }
static uint32_t child(unsigned char *b, uint32_t i) { return xb_get_le32(entry(b, i)); }
static uint32_t recno_of(unsigned char *b, uint32_t i) { return xb_get_le32(entry(b, i) + 4); }
static unsigned char *key_of(unsigned char *b, uint32_t i) { return entry(b, i) + NTX_ENTRY_HEAD; }

int xb_ntx_compare(const xb_ntx_info *info, const unsigned char *a, const unsigned char *b, size_t len) {
  int c = memcmp(a, b, len);
  return info->descend ? -c : c;
}

/* (key, recno) against entry i of a page. */
static int compare_entry(const xb_ntx *x, const unsigned char *key, uint32_t recno, unsigned char *b, uint32_t i) {
  int c = xb_ntx_compare(&x->info, key, key_of(b, i), x->info.keyLen);
  if (c) return c;
  uint32_t r = recno_of(b, i);
  return recno < r ? -1 : recno > r;
}

static uint32_t max_entries(uint32_t entrySize) {
  uint32_t max = 1022 / (entrySize + 2) - 1;
  return max & ~1u;
}

/* An empty page: its offsets in order. */
static void clear_page(const xb_ntx *x, unsigned char *b) {
  memset(b, 0, NTX_PAGE);
  for (uint32_t i = 0; i <= x->max; ++i) xb_put_le16(b + 2 + 2 * i, (uint16_t)(x->base + i * x->entrySize));
}

/* Writes `n` entries (and the one after them, n + 1 in all, `entrySize` bytes each from `src`) into a page, its
 * offsets in order. */
static void store(const xb_ntx *x, unsigned char *b, const unsigned char *src, uint32_t n) {
  clear_page(x, b);
  xb_put_le16(b, (uint16_t)n);
  memcpy(b + x->base, src, (size_t)(n + 1) * x->entrySize);
  memset(b + x->base + (size_t)n * x->entrySize + 4, 0, x->entrySize - 4); /* the last: only its page offset */
}

/* Copies a page's entries (and the one after them) to `dst`: their count. */
static uint32_t load(const xb_ntx *x, unsigned char *b, unsigned char *dst) {
  uint32_t n = count(b);
  for (uint32_t i = 0; i <= n; ++i) memcpy(dst + (size_t)i * x->entrySize, entry(b, i), x->entrySize);
  return n;
}

/* Whether a header (of a file `end` bytes long) is an NTX one: its sizes agree, its pages are in the file. */
static int header_valid(const unsigned char *h, int64_t end, uint32_t *entrySize, uint32_t *max) {
  uint32_t sig = xb_get_le16(h), keyLen = xb_get_le16(h + NTX_KEY_LEN);
  uint32_t root = xb_get_le32(h + NTX_ROOT), freeList = xb_get_le32(h + NTX_FREE);
  *entrySize = xb_get_le16(h + NTX_ENTRY_SIZE);
  *max = xb_get_le16(h + NTX_MAX);
  return (sig == NTX_SIG || sig == NTX_SIG_COND) && keyLen >= 1 && keyLen <= XB_NTX_KEY_MAX &&
         *entrySize == keyLen + NTX_ENTRY_HEAD && *max >= 2 && *max == max_entries(*entrySize) &&
         xb_get_le16(h + NTX_HALF) == *max / 2 && root >= NTX_PAGE && root % NTX_PAGE == 0 && root < end &&
         freeList % NTX_PAGE == 0 && freeList < end && end <= UINT32_MAX;
}

/* Whether a page read from the file can be used: its count and offsets inside the page. */
static int page_valid(const xb_ntx *x, const unsigned char *b) {
  if (count(b) > x->max) return 0;
  for (uint32_t i = 0; i <= x->max; ++i) {
    uint32_t o = xb_get_le16(b + 2 + 2 * i);
    if (o < x->base || o + x->entrySize > NTX_PAGE) return 0;
  }
  return 1;
}

static int write_page(xb_ntx *x, page *p, int32_t *err) {
  if (!xb_platform_file_write_at(x->f, p->offset, p->b, NTX_PAGE, err)) return 0;
  p->dirty = 0;
  return 1;
}

static uint32_t bucket(uint32_t offset) { return (offset / NTX_PAGE * 2654435761u) >> (32 - NTX_BUCKET_BITS); }

/* Takes slot `p` out of its hash bucket. */
static void unhash(xb_ntx *x, page *p) {
  uint16_t slot = (uint16_t)(p - x->cache), *link = &x->buckets[bucket(p->offset)];
  while (*link != NTX_NONE && *link != slot) link = &x->cache[*link].next;
  if (*link == slot) *link = p->next;
}

/* Gives slot `p` the page at `offset`. */
static void rehash(xb_ntx *x, page *p, uint32_t offset) {
  if (p->offset) unhash(x, p);
  p->offset = offset;
  uint32_t b = bucket(offset);
  p->next = x->buckets[b];
  x->buckets[b] = (uint16_t)(p - x->cache);
}

/* A slot of the cache for a page: a free one, else the one unused longest (written first when it changed). */
static page *victim(xb_ntx *x, int32_t *err) {
  page *best = NULL;
  for (uint32_t i = 0; i < NTX_CACHE; ++i) {
    page *p = &x->cache[i];
    if (p->pins) continue;
    if (!p->offset) return p;
    if (!best || p->used < best->used) best = p;
  }
  if (!best) { /* every page pinned: an operation never holds that many */
    *err = XB_FERR_FAILURE;
    return NULL;
  }
  if (best->dirty && !write_page(x, best, err)) return NULL;
  unhash(x, best);
  best->offset = 0;
  return best;
}

/* The page at `offset`, pinned (release it). */
static page *get(xb_ntx *x, uint32_t offset, int32_t *err) {
  if (offset < NTX_PAGE || offset % NTX_PAGE || offset >= x->end) {
    *err = XB_FERR_FAILURE;
    return NULL;
  }
  page *hint = &x->cache[x->hint];
  if (hint->offset == offset) { /* the page used last: navigation stays on a leaf */
    hint->used = ++x->clock;
    ++hint->pins;
    return hint;
  }
  for (uint16_t i = x->buckets[bucket(offset)]; i != NTX_NONE; i = x->cache[i].next)
    if (x->cache[i].offset == offset) {
      page *p = &x->cache[i];
      x->hint = i;
      p->used = ++x->clock;
      ++p->pins;
      return p;
    }
  page *p = victim(x, err);
  if (!p) return NULL;
  if (!xb_file_read_at(x->f, offset, p->b, NTX_PAGE, err)) return NULL;
  if (!page_valid(x, p->b)) {
    *err = XB_FERR_FAILURE;
    return NULL;
  }
  rehash(x, p, offset);
  p->used = ++x->clock;
  p->pins = 1;
  p->dirty = 0;
  x->hint = (uint32_t)(p - x->cache);
  return p;
}

static void release(page *p) { --p->pins; }

static void touched(xb_ntx *x, page *p) {
  p->dirty = 1;
  x->changed = 1;
}

/* A page for new entries, empty and pinned: the first free one, else one at the end of the file. */
static page *new_page(xb_ntx *x, int32_t *err) {
  page *p;
  if (x->freeList) {
    p = get(x, x->freeList, err);
    if (!p) return NULL;
    x->freeList = child(p->b, 0);
  } else {
    if (x->end > UINT32_MAX - NTX_PAGE) {
      *err = XB_FERR_FAILURE;
      return NULL;
    }
    p = victim(x, err);
    if (!p) return NULL;
    rehash(x, p, x->end);
    x->end += NTX_PAGE;
    p->used = ++x->clock;
    p->pins = 1;
  }
  clear_page(x, p->b);
  touched(x, p);
  x->headerDirty = 1;
  return p;
}

static void free_page(xb_ntx *x, page *p) {
  clear_page(x, p->b);
  xb_put_le32(entry(p->b, 0), x->freeList);
  x->freeList = p->offset;
  touched(x, p);
  x->headerDirty = 1;
}

static void put_header(const xb_ntx_info *info, uint32_t entrySize, uint32_t max, uint32_t root, uint32_t freeList,
                       uint16_t version, unsigned char *h) {
  memset(h, 0, NTX_PAGE);
  xb_put_le16(h, info->cond[0] ? NTX_SIG_COND : NTX_SIG);
  xb_put_le16(h + NTX_VERSION, version);
  xb_put_le32(h + NTX_ROOT, root);
  xb_put_le32(h + NTX_FREE, freeList);
  xb_put_le16(h + NTX_ENTRY_SIZE, (uint16_t)entrySize);
  xb_put_le16(h + NTX_KEY_LEN, (uint16_t)info->keyLen);
  xb_put_le16(h + NTX_KEY_DEC, (uint16_t)info->keyDec);
  xb_put_le16(h + NTX_MAX, (uint16_t)max);
  xb_put_le16(h + NTX_HALF, (uint16_t)(max / 2));
  memcpy(h + NTX_KEY_TEXT, info->key, strlen(info->key));
  h[NTX_UNIQUE] = (unsigned char)(info->unique != 0);
  h[NTX_DESCEND] = (unsigned char)(info->descend != 0);
  memcpy(h + NTX_COND_TEXT, info->cond, strlen(info->cond));
}

/* The bulk build. Entries stream in, in order, into a page per level: a page that is full when an entry comes is
 * finished (its offset given), and the entry goes up to the level above as the one after it. At the end, from the
 * bottom, the last page of a level with fewer than half the entries takes, with the page before it and the entry
 * between them, an even share (the first page one more). The last pages get their offsets from the bottom up; the
 * root is the top one's. */
typedef struct level {
  unsigned char *cur, *prev; /* the pages' slots (max + 1 each, entrySize bytes) */
  uint16_t *order;           /* the slot of each entry of `cur`, in order (prev's: each in its own) */
  uint32_t ncur, nprev;
  uint32_t prevOffset;       /* 0: no page finished at this level yet */
} level;

typedef struct builder {
  xb_ntx *x;                 /* entrySize, max and base only */
  level levels[NTX_DEPTH];
  uint32_t nlevels, next;    /* the offset the next finished page gets */
  xb_file f;
  unsigned char page[NTX_PAGE];
  int32_t *err;
} builder;

/* Writes a page of the build: `n` entries, every slot as the level's buffer holds it (a page shared at the end keeps
 * what its freed slots held, as the reference's pages do), the slots in `order` (NULL: each entry in its own). */
static int write_entries(builder *bd, uint32_t offset, const unsigned char *slots, const uint16_t *order, uint32_t n) {
  xb_ntx *x = bd->x;
  clear_page(x, bd->page);
  xb_put_le16(bd->page, (uint16_t)n);
  if (order)
    for (uint32_t i = 0; i <= x->max; ++i)
      xb_put_le16(bd->page + 2 + 2 * i, (uint16_t)(x->base + order[i] * x->entrySize));
  memcpy(bd->page + x->base, slots, (size_t)(x->max + 1) * x->entrySize);
  return xb_platform_file_write_at(bd->f, offset, bd->page, NTX_PAGE, bd->err);
}

static void identity(uint16_t *order, uint32_t max) {
  for (uint32_t i = 0; i <= max; ++i) order[i] = (uint16_t)i;
}

/* The slot of entry `i` of a level's last page. */
static unsigned char *cur_entry(const builder *bd, const level *v, uint32_t i) {
  return v->cur + (size_t)v->order[i] * bd->x->entrySize;
}

static level *level_at(builder *bd, uint32_t l) {
  if (l == bd->nlevels) {
    level *v = &bd->levels[bd->nlevels++];
    size_t size = (size_t)(bd->x->max + 1) * bd->x->entrySize;
    v->cur = calloc(1, size);
    v->prev = calloc(1, size);
    v->order = malloc((bd->x->max + 1) * sizeof *v->order);
    if (!v->cur || !v->prev || !v->order) xb_gc_fatal("out of memory");
    identity(v->order, bd->x->max);
    v->ncur = v->nprev = v->prevOffset = 0;
  }
  return &bd->levels[l];
}

/* Adds an entry (`e`: entrySize bytes, its page offset already set) to level `l`. */
static int add(builder *bd, uint32_t l, unsigned char *e) {
  if (l >= NTX_DEPTH) {
    *bd->err = XB_FERR_FAILURE;
    return 0;
  }
  xb_ntx *x = bd->x;
  level *v = level_at(bd, l);
  if (v->ncur < x->max) {
    memcpy(v->cur + (size_t)v->ncur * x->entrySize, e, x->entrySize);
    ++v->ncur;
    return 1;
  }
  xb_put_le32(v->cur + (size_t)v->ncur * x->entrySize, xb_get_le32(e)); /* its page: the one after the last entry */
  if (v->prevOffset && !write_entries(bd, v->prevOffset, v->prev, NULL, v->nprev)) return 0;
  unsigned char *t = v->prev;
  v->prev = v->cur;
  v->cur = t;
  v->nprev = v->ncur;
  v->ncur = 0;
  memset(v->cur, 0, (size_t)(x->max + 1) * x->entrySize);
  if (bd->next > UINT32_MAX - NTX_PAGE) { /* past the 4 GB a page offset reaches */
    *bd->err = XB_FERR_FAILURE;
    return 0;
  }
  v->prevOffset = bd->next;
  bd->next += NTX_PAGE;
  xb_put_le32(e, v->prevOffset);
  return add(bd, l + 1, e);
}

/* The entry between the last two pages of level `l`: the last one added to a level above. */
static unsigned char *separator(builder *bd, uint32_t l) {
  uint32_t m = l + 1;
  while (bd->levels[m].ncur == 0 && m + 1 < bd->nlevels) ++m;
  return cur_entry(bd, &bd->levels[m], bd->levels[m].ncur - 1);
}

/* Shares the entries of the last two pages of level `l` evenly (the first page one more), as the reference lays it
 * out: the first page keeps its first entries where they are; the k entries that go to the second page (the first
 * page's last ones, then the one between the pages) take its free slots F .. F + k - 1, turned by F mod k (entry p
 * in slot F + (p - F) mod k), ahead of its own entries in the order. */
static void rebalance(builder *bd, uint32_t l) {
  xb_ntx *x = bd->x;
  level *v = &bd->levels[l];
  size_t es = x->entrySize;
  unsigned char *sep = separator(bd, l);
  uint32_t total = v->nprev + 1 + v->ncur, left = total / 2; /* the entries; the first page's share */
  if (left >= v->nprev) return;
  uint32_t tail = v->nprev - left - 1;                     /* the first page's entries that move */
  uint32_t sepSlot = v->ncur + 1, n = v->ncur;              /* the second page's first free slot */
  uint32_t k = tail + 1, turn = sepSlot % k; /* the entries that move, in order: the tail, then the separator */
  uint16_t *o = v->order;                    /* in order until now: one entry per slot */
  for (uint32_t p = 0; p < k; ++p) {
    uint32_t slot = sepSlot + (p + k - turn) % k;
    unsigned char *dst = v->cur + (size_t)slot * es;
    if (p < tail) {
      memcpy(dst, v->prev + (size_t)(left + 1 + p) * es, es);
    } else { /* the entry between the pages comes down, its page the first page's last one */
      memcpy(dst, sep, es);
      xb_put_le32(dst, xb_get_le32(v->prev + (size_t)v->nprev * es));
    }
    *o++ = (uint16_t)slot;
  }
  for (uint32_t p = 0; p <= n; ++p) *o++ = (uint16_t)p;
  for (uint32_t p = sepSlot + k; p <= x->max; ++p) *o++ = (uint16_t)p;
  memcpy(sep + 4, v->prev + (size_t)left * es + 4, es - 4); /* the new entry between them; its page stays */
  v->nprev = left;
  v->ncur = total - left - 1;
}

int xb_ntx_create(const char *path, const xb_ntx_info *info, const unsigned char *keys, const uint32_t *recnos,
                  uint32_t count_, int32_t *err) {
  xb_ntx x;
  memset(&x, 0, sizeof x);
  x.info = *info;
  x.entrySize = info->keyLen + NTX_ENTRY_HEAD;
  x.max = max_entries(x.entrySize);
  x.base = 2 + 2 * (x.max + 1);
  builder bd;
  memset(&bd, 0, sizeof bd);
  bd.x = &x;
  bd.next = NTX_PAGE;
  bd.err = err;
  unsigned char *e = malloc(x.entrySize);
  if (!e) xb_gc_fatal("out of memory");
  xb_shared *sf = NULL;
  int ok = xb_shared_open(path, 0, 1, 1, &sf, err);
  if (ok) {
    bd.f = xb_shared_handle(sf);
    level_at(&bd, 0);
    for (uint32_t i = 0; ok && i < count_; ++i) {
      memset(e, 0, NTX_ENTRY_HEAD);
      xb_put_le32(e + 4, recnos[i]);
      memcpy(e + NTX_ENTRY_HEAD, keys + (size_t)i * info->keyLen, info->keyLen);
      ok = add(&bd, 0, e);
    }
    for (uint32_t l = 0; ok && l + 1 < bd.nlevels; ++l)
      if (bd.levels[l].ncur < x.max / 2 && bd.levels[l].prevOffset) rebalance(&bd, l);
    uint32_t root = 0;
    for (uint32_t l = 0; ok && l < bd.nlevels; ++l) { /* the last pages: offsets from the bottom up */
      level *v = &bd.levels[l];
      if (l > 0) xb_put_le32(cur_entry(&bd, v, v->ncur), root); /* the page below, last of its level */
      if (bd.next > UINT32_MAX - NTX_PAGE) {
        *err = XB_FERR_FAILURE;
        ok = 0;
        break;
      }
      root = bd.next;
      bd.next += NTX_PAGE;
      ok = (!v->prevOffset || write_entries(&bd, v->prevOffset, v->prev, NULL, v->nprev)) &&
           write_entries(&bd, root, v->cur, v->order, v->ncur);
    }
    unsigned char h[NTX_PAGE];
    put_header(info, x.entrySize, x.max, root, 0, 1, h);
    ok = ok && xb_platform_file_write_at(bd.f, 0, h, NTX_PAGE, err);
    int32_t closeErr = 0;
    if (!xb_shared_close(sf, &closeErr) && ok) {
      *err = closeErr;
      ok = 0;
    }
  }
  for (uint32_t l = 0; l < bd.nlevels; ++l) {
    free(bd.levels[l].cur);
    free(bd.levels[l].prev);
    free(bd.levels[l].order);
  }
  free(e);
  return ok;
}

int xb_ntx_open(const char *path, int readonly, int exclusive, xb_ntx **out, xb_ntx_info *info, int32_t *err) {
  xb_shared *sf;
  if (!xb_shared_open(path, readonly, exclusive, 0, &sf, err)) return 0;
  xb_file f = xb_shared_handle(sf);
  unsigned char h[NTX_PAGE];
  uint32_t entrySize, max;
  int64_t end = -1;
  if (!xb_file_read_at(f, 0, h, NTX_PAGE, err) || (end = xb_platform_file_seek(f, 0, FS_END, err)) < 0 ||
      !header_valid(h, end, &entrySize, &max)) {
    if (end >= 0) *err = XB_FERR_FAILURE;
    int32_t ignored;
    xb_shared_close(sf, &ignored);
    return 0;
  }
  xb_ntx *x = calloc(1, sizeof *x);
  if (!x) xb_gc_fatal("out of memory");
  x->s = sf;
  x->f = f;
  x->shared = !exclusive;
  x->readonly = readonly;
  memcpy(x->info.key, h + NTX_KEY_TEXT, XB_NTX_KEY_MAX);
  memcpy(x->info.cond, h + NTX_COND_TEXT, XB_NTX_KEY_MAX);
  x->info.keyLen = xb_get_le16(h + NTX_KEY_LEN);
  x->info.keyDec = xb_get_le16(h + NTX_KEY_DEC);
  x->info.unique = h[NTX_UNIQUE] != 0;
  x->info.descend = h[NTX_DESCEND] != 0;
  x->entrySize = entrySize;
  x->max = max;
  x->half = max / 2;
  x->base = 2 + 2 * (max + 1);
  x->version = xb_get_le16(h + NTX_VERSION);
  x->root = xb_get_le32(h + NTX_ROOT);
  x->freeList = xb_get_le32(h + NTX_FREE);
  x->end = (uint32_t)(end / NTX_PAGE * NTX_PAGE);
  x->cache = calloc(NTX_CACHE, sizeof *x->cache);
  x->scratch = malloc((size_t)(2 * max + 4) * entrySize);
  if (!x->cache || !x->scratch) xb_gc_fatal("out of memory");
  for (uint32_t i = 0; i < NTX_BUCKETS; ++i) x->buckets[i] = NTX_NONE;
  *info = x->info;
  *out = x;
  return 1;
}

int xb_ntx_flush(xb_ntx *x, int32_t *err) {
  for (uint32_t i = 0; i < NTX_CACHE; ++i)
    if (x->cache[i].offset && x->cache[i].dirty && !write_page(x, &x->cache[i], err)) return 0;
  if (!x->changed && !x->headerDirty) return 1;
  if (x->changed) ++x->version;
  unsigned char h[NTX_PAGE];
  put_header(&x->info, x->entrySize, x->max, x->root, x->freeList, x->version, h);
  if (!xb_platform_file_write_at(x->f, 0, h, NTX_COUNTS, err)) return 0;
  x->changed = x->headerDirty = 0;
  return 1;
}

int xb_ntx_close(xb_ntx *x, int32_t *err) {
  int ok = x->readonly || xb_ntx_flush(x, err);
  int32_t closeErr = 0;
  if (!xb_shared_close(x->s, &closeErr) && ok) {
    *err = closeErr;
    ok = 0;
  }
  free(x->cache);
  free(x->scratch);
  free(x);
  return ok;
}

/* The cursor on entry (path's last page, pos): its record number. */
static int on_entry(xb_ntx *x, uint32_t *recno, int32_t *err) {
  page *p = get(x, x->path[x->depth - 1].page, err);
  if (!p) return -1;
  *recno = x->onRecno = recno_of(p->b, x->path[x->depth - 1].pos);
  if (x->shared) memcpy(x->onKey, key_of(p->b, x->path[x->depth - 1].pos), x->info.keyLen);
  release(p);
  x->on = 1;
  x->placed = x->changes;
  return 1;
}

/* From the last level of the path, whose pos is at or past its count: up to the first ancestor with an entry at its
 * pos, which the cursor is then on (`back`: the one before its pos). 0 when there is none. */
static int climb(xb_ntx *x, int back, uint32_t *recno, int32_t *err) {
  while (x->depth > 1) {
    --x->depth;
    page *p = get(x, x->path[x->depth - 1].page, err);
    if (!p) return -1;
    uint32_t n = count(p->b), q = x->path[x->depth - 1].pos;
    release(p);
    if (back ? q > 0 : q < n) {
      if (back) x->path[x->depth - 1].pos = q - 1;
      return on_entry(x, recno, err);
    }
  }
  x->on = 0;
  return 0;
}

/* Down from page `offset` to a leaf along the first (`last`: the last) children, pushing the path. */
static int edge(xb_ntx *x, uint32_t offset, int last, int32_t *err) {
  for (;;) {
    if (x->depth >= NTX_DEPTH) {
      *err = XB_FERR_FAILURE;
      return 0;
    }
    page *p = get(x, offset, err);
    if (!p) return 0;
    uint32_t n = count(p->b), pos = last ? n : 0;
    x->path[x->depth].page = offset;
    x->path[x->depth].pos = pos;
    ++x->depth;
    offset = child(p->b, pos);
    release(p);
    if (!offset) {
      if (last) x->path[x->depth - 1].pos = n ? n - 1 : 0;
      return 1;
    }
  }
}

/* Places the cursor at the leaf the edge leads to: its first (last) entry, or past it when the leaf is empty. */
static int end_entry(xb_ntx *x, int last, uint32_t *recno, int32_t *err) {
  page *p = get(x, x->path[x->depth - 1].page, err);
  if (!p) return -1;
  uint32_t n = count(p->b);
  release(p);
  if (n) return on_entry(x, recno, err);
  if (!last) x->path[x->depth - 1].pos = 0;
  return climb(x, last, recno, err);
}

static int go_first(xb_ntx *x, uint32_t *recno, int32_t *err) {
  x->depth = 0;
  return edge(x, x->root, 0, err) ? end_entry(x, 0, recno, err) : -1;
}

static int go_last(xb_ntx *x, uint32_t *recno, int32_t *err) {
  x->depth = 0;
  return edge(x, x->root, 1, err) ? end_entry(x, 1, recno, err) : -1;
}

static int go_next(xb_ntx *x, uint32_t *recno, int32_t *err) {
  page *p = get(x, x->path[x->depth - 1].page, err);
  if (!p) return -1;
  uint32_t pos = x->path[x->depth - 1].pos, n = count(p->b), right = child(p->b, pos + 1);
  release(p);
  if (right) {
    x->path[x->depth - 1].pos = pos + 1;
    return edge(x, right, 0, err) ? end_entry(x, 0, recno, err) : -1;
  }
  x->path[x->depth - 1].pos = pos + 1;
  return pos + 1 < n ? on_entry(x, recno, err) : climb(x, 0, recno, err);
}

static int go_prev(xb_ntx *x, uint32_t *recno, int32_t *err) {
  page *p = get(x, x->path[x->depth - 1].page, err);
  if (!p) return -1;
  uint32_t pos = x->path[x->depth - 1].pos, left = child(p->b, pos);
  release(p);
  if (left) return edge(x, left, 1, err) ? end_entry(x, 1, recno, err) : -1;
  if (pos > 0) {
    x->path[x->depth - 1].pos = pos - 1;
    return on_entry(x, recno, err);
  }
  return climb(x, 1, recno, err);
}

static int before(const xb_ntx *x, const xb_ntx_bound *t, unsigned char *b, uint32_t i) {
  if (t->whole) return compare_entry(x, t->key, t->recno, b, i) > 0;
  int c = xb_ntx_compare(&x->info, key_of(b, i), t->key, t->len);
  return t->past ? c <= 0 : c < 0;
}

/* The index in page buffer `b` of its first entry not before the bound (its count when there is none). */
static uint32_t lower_bound(const xb_ntx *x, const xb_ntx_bound *t, unsigned char *b) {
  uint32_t lo = 0, hi = count(b);
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (before(x, t, b, mid)) lo = mid + 1;
    else hi = mid;
  }
  return lo;
}

/* Places the cursor on the first entry not before the bound; 0 when every entry is. */
static int descend(xb_ntx *x, const xb_ntx_bound *t, uint32_t *recno, int32_t *err) {
  uint32_t offset = x->root;
  x->depth = 0;
  for (;;) {
    if (x->depth >= NTX_DEPTH) {
      *err = XB_FERR_FAILURE;
      return -1;
    }
    page *p = get(x, offset, err);
    if (!p) return -1;
    uint32_t lo = lower_bound(x, t, p->b);
    x->path[x->depth].page = offset;
    x->path[x->depth].pos = lo;
    ++x->depth;
    uint32_t n = count(p->b);
    offset = child(p->b, lo);
    release(p);
    if (!offset) return lo < n ? on_entry(x, recno, err) : climb(x, 0, recno, err);
  }
}

static int seek_key(xb_ntx *x, const unsigned char *key, size_t len, int last, uint32_t *recno, int32_t *err) {
  xb_ntx_bound t = {key, len < x->info.keyLen ? len : x->info.keyLen, 0, 0, last};
  int r = descend(x, &t, recno, err);
  if (!last) return r;
  if (r == 0) return go_last(x, recno, err);
  return r < 0 ? r : go_prev(x, recno, err);
}

/* Equal keys come in record order here; a file another program kept may have them in the order they came in.
 * When the search by key and record misses, the record's entry is looked for among the entries of its key. */
static int find_entry(xb_ntx *x, const unsigned char *key, uint32_t recno, uint32_t *found, int32_t *err) {
  xb_ntx_bound t = {key, x->info.keyLen, recno, 1, 0};
  int r = descend(x, &t, found, err);
  if (r < 0 || (r > 0 && *found == recno)) return r;
  r = seek_key(x, key, x->info.keyLen, 0, found, err);
  while (r > 0 && *found != recno) {
    const unsigned char *k = xb_ntx_key(x);
    if (!k || memcmp(k, key, x->info.keyLen) != 0) break;
    r = go_next(x, found, err);
  }
  if (r > 0 && *found == recno) return r;
  return r < 0 ? r : descend(x, &t, found, err); /* not there: where it would be */
}

/* Page `offset`, `depth` levels below the root, and its entry count. A tree deeper than NTX_DEPTH, or a count that
 * would read more pages than the file has (a loop), is a broken file.
 * The callers copy the children's offsets and release it before reading them: a deep tree never pins more than its
 * path. */
static page *get_level(xb_ntx *x, uint32_t offset, uint32_t depth, uint32_t *n, int32_t *err) {
  if (depth >= NTX_DEPTH || x->visits == 0) {
    *err = XB_FERR_FAILURE;
    return NULL;
  }
  --x->visits;
  page *p = get(x, offset, err);
  if (p) *n = count(p->b);
  return p;
}

/* Adds the entries of the tree under page `offset` to `n` (`depth`: the levels above it). */
static int count_under(xb_ntx *x, uint32_t offset, uint32_t depth, uint64_t *n, int32_t *err) {
  uint32_t entries, children[NTX_CHILDREN], nchildren = 0;
  page *p = get_level(x, offset, depth, &entries, err);
  if (!p) return 0;
  for (uint32_t i = 0; i <= entries; ++i)
    if (child(p->b, i)) children[nchildren++] = child(p->b, i);
  release(p);
  *n += entries;
  for (uint32_t i = 0; i < nchildren; ++i)
    if (!count_under(x, children[i], depth + 1, n, err)) return 0;
  return 1;
}

/* Adds to `n` the entries under page `offset` not before `lo` and before `hi` (NULL: no bound that side), or, with
 * `cursor`, before the cursor's entry, whose path goes through this page. Only the pages that hold the bounds are
 * searched; the subtrees between them are counted whole. */
static int count_between(xb_ntx *x, uint32_t offset, uint32_t depth, const xb_ntx_bound *lo, const xb_ntx_bound *hi,
                         int cursor, uint64_t *n, int32_t *err) {
  if (!lo && !hi && !cursor) return count_under(x, offset, depth, n, err);
  uint32_t entries;
  page *p = get_level(x, offset, depth, &entries, err);
  if (!p) return 0;
  /* The cursor's path holds the child descended at the levels above its entry, which follows that child. */
  int below = cursor && depth + 1 < x->depth;
  uint32_t a = lo ? lower_bound(x, lo, p->b) : 0;
  uint32_t b = cursor ? x->path[depth].pos : hi ? lower_bound(x, hi, p->b) : entries;
  uint32_t children[NTX_CHILDREN];
  for (uint32_t i = a; i <= b; ++i) children[i - a] = child(p->b, i);
  release(p);
  if (a > b) return 1; /* the bounds cross: nothing between them */
  *n += b - a;
  for (uint32_t i = a; i <= b; ++i) {
    int bounded = i == b && (cursor ? below : hi != NULL);
    if (children[i - a] && !count_between(x, children[i - a], depth + 1, i == a ? lo : NULL,
                                          bounded && !cursor ? hi : NULL, bounded && cursor, n, err))
      return 0;
  }
  return 1;
}

static int count_range(xb_ntx *x, const xb_ntx_bound *lo, const xb_ntx_bound *hi, uint64_t *n, int32_t *err) {
  *n = 0;
  x->visits = x->end / NTX_PAGE + 1;
  return count_between(x, x->root, 0, lo, hi, 0, n, err);
}

static int count_to_cursor(xb_ntx *x, const xb_ntx_bound *lo, uint64_t *n, int32_t *err) {
  *n = 0;
  x->visits = x->end / NTX_PAGE + 1;
  return x->on ? count_between(x, x->root, 0, lo, NULL, 1, n, err) : 1;
}

/* Places the cursor on entry `*k` (from 0) among those under page `offset` not before `lo` (NULL: all), the path
 * above it at levels 0 .. depth - 1: 1 on it, 0 when there are fewer (`*k` less those there are), -1 after an
 * error. */
static int select_from(xb_ntx *x, uint32_t offset, uint32_t depth, const xb_ntx_bound *lo, uint64_t *k,
                       uint32_t *recno, int32_t *err) {
  uint32_t entries;
  page *p = get_level(x, offset, depth, &entries, err);
  if (!p) return -1;
  uint32_t a = lo ? lower_bound(x, lo, p->b) : 0;
  uint32_t children[NTX_CHILDREN];
  for (uint32_t i = a; i <= entries; ++i) children[i - a] = child(p->b, i);
  release(p);
  x->path[depth].page = offset;
  for (uint32_t i = a; i <= entries; ++i) {
    x->path[depth].pos = i;
    uint32_t c = children[i - a];
    if (c) { /* found there, or `*k` less its entries (the first child: those not before the bound) */
      int r = select_from(x, c, depth + 1, i == a ? lo : NULL, k, recno, err);
      if (r) return r;
    }
    if (i == entries) break;
    if (*k == 0) {
      x->depth = depth + 1;
      return on_entry(x, recno, err);
    }
    --*k;
  }
  return 0;
}

static int select_entry(xb_ntx *x, const xb_ntx_bound *lo, uint64_t k, uint32_t *recno, int32_t *err) {
  x->visits = x->end / NTX_PAGE + 1;
  int r = select_from(x, x->root, 0, lo, &k, recno, err);
  if (r <= 0) x->on = 0;
  return r;
}

const unsigned char *xb_ntx_key(xb_ntx *x) {
  if (!x->on) return NULL;
  if (x->shared) return x->onKey; /* the pages may go at the next operation */
  int32_t err;
  page *p = get(x, x->path[x->depth - 1].page, &err);
  if (!p) return NULL;
  const unsigned char *k = key_of(p->b, x->path[x->depth - 1].pos); /* stays: the page is in the cache */
  release(p);
  return k;
}

int xb_ntx_at(const xb_ntx *x, uint32_t recno) { return x->on && x->placed == x->changes && x->onRecno == recno; }

/* Whether the page at path level `d` is the last (`last`) or the first of its level: every page above it on the path
 * went down its last (first) child. -1 after an error. */
static int at_edge(xb_ntx *x, uint32_t d, int last, int32_t *err) {
  for (uint32_t k = 0; k < d; ++k) {
    if (!last) {
      if (x->path[k].pos != 0) return 0;
      continue;
    }
    page *p = get(x, x->path[k].page, err);
    if (!p) return -1;
    int edge = x->path[k].pos == count(p->b);
    release(p);
    if (!edge) return 0;
  }
  return 1;
}

/* The page at path level `d` (`p`, pinned) overflows with the `total` entries in `all` (and the one after them): when
 * its left sibling has room, the first entry goes up to the parent and the parent's entry between them down to the
 * end of the sibling; else the same to the right with the last entry. 1 when done, 0 when no sibling has room, -1
 * after an error. */
static int shift_to_sibling(xb_ntx *x, uint32_t d, page *p, unsigned char *all, uint32_t total, int32_t *err) {
  size_t es = x->entrySize;
  page *parent = get(x, x->path[d - 1].page, err);
  if (!parent) return -1;
  uint32_t ci = x->path[d - 1].pos, pn = count(parent->b);
  for (int side = 0; side < 2; ++side) {
    int toLeft = side == 0;
    if (toLeft ? ci == 0 : ci >= pn) continue;
    uint32_t si = toLeft ? ci - 1 : ci; /* the parent's entry between the two pages */
    page *s = get(x, child(parent->b, toLeft ? ci - 1 : ci + 1), err);
    if (!s) {
      release(parent);
      return -1;
    }
    uint32_t sn = count(s->b);
    if (sn >= x->max) {
      release(s);
      continue;
    }
    unsigned char *sep = entry(parent->b, si), *sib = x->scratch + (size_t)(total + 1) * es;
    load(x, s->b, sib); /* sn entries and the one after them */
    if (toLeft) { /* the separator after the sibling's entries, the page's first entry up */
      memcpy(sib + sn * es + 4, sep + 4, es - 4);
      xb_put_le32(sib + (sn + 1) * es, xb_get_le32(all));
      memset(sib + (sn + 1) * es + 4, 0, es - 4);
      memcpy(sep + 4, all + 4, es - 4);
      store(x, s->b, sib, sn + 1);
      store(x, p->b, all + es, total - 1);
    } else { /* the separator before the sibling's entries, the page's last entry up */
      unsigned char *last = all + (size_t)(total - 1) * es, *after = all + (size_t)total * es;
      memmove(sib + es, sib, (size_t)(sn + 1) * es);
      xb_put_le32(sib, xb_get_le32(after));
      memcpy(sib + 4, sep + 4, es - 4);
      memcpy(sep + 4, last + 4, es - 4);
      xb_put_le32(after, 0);
      memset(last + 4, 0, es - 4); /* the last entry's page is now the one after the page's entries */
      store(x, s->b, sib, sn + 1);
      store(x, p->b, all, total - 1);
    }
    touched(x, s);
    touched(x, p);
    touched(x, parent);
    release(s);
    release(parent);
    return 1;
  }
  release(parent);
  return 0;
}

/* Inserts entry `e` (its page offset set) at `pos` of the page at path level `d`, and `right` as the page after it
 * (in a branch: the right half of the page it was split from). Splits up the path as needed. */
static int insert_at(xb_ntx *x, uint32_t d, uint32_t pos, unsigned char *e, uint32_t right, int32_t *err) {
  page *p = get(x, x->path[d].page, err);
  if (!p) return 0;
  uint32_t n = count(p->b);
  size_t es = x->entrySize;
  if (n < x->max) {
    unsigned char *b = p->b;
    uint16_t freeSlot = xb_get_le16(b + 2 + 2 * (n + 1));
    memmove(b + 2 + 2 * (pos + 1), b + 2 + 2 * pos, 2 * (size_t)(n + 1 - pos));
    xb_put_le16(b + 2 + 2 * pos, freeSlot);
    memcpy(b + freeSlot, e, es);
    if (right) xb_put_le32(entry(b, pos + 1), right);
    xb_put_le16(b, (uint16_t)(n + 1));
    touched(x, p);
    release(p);
    return 1;
  }
  unsigned char *all = x->scratch; /* the max + 1 entries, and the one after them */
  load(x, p->b, all);
  memmove(all + (pos + 1) * es, all + pos * es, (size_t)(n + 1 - pos) * es);
  memcpy(all + pos * es, e, es);
  if (right) xb_put_le32(all + (pos + 1) * es, right);
  /* Where the page splits: halves; but at the end of the last page of its level (keys added in order) the left
   * page keeps all it can, at the start of the first one the right page does, so that pages filled in order stay
   * full. */
  int edge = pos == n || pos == 0 ? at_edge(x, d, pos == n, err) : 0;
  if (edge < 0) {
    release(p);
    return 0;
  }
  uint32_t total = n + 1, left = !edge ? x->half : pos == n ? total - 2 : 1;
  if (!edge && d > 0) { /* a sibling with room takes an entry through the parent instead of a split */
    int moved = shift_to_sibling(x, d, p, all, total, err);
    if (moved != 0) {
      release(p);
      return moved > 0;
    }
  }
  page *r = new_page(x, err);
  if (!r) {
    release(p);
    return 0;
  }
  unsigned char *mid = all + (size_t)(2 * x->max + 2) * es; /* the entry that goes up */
  memcpy(mid, all + left * es, es);
  store(x, r->b, all + (left + 1) * es, total - left - 1);
  unsigned char *leftEnd = all + left * es;
  memset(leftEnd + 4, 0, es - 4); /* the left half's last: the page before the entry that goes up */
  store(x, p->b, all, left);
  touched(x, p);
  xb_put_le32(mid, p->offset);
  uint32_t rightOffset = r->offset;
  release(r);
  release(p);
  if (d > 0) return insert_at(x, d - 1, x->path[d - 1].pos, mid, rightOffset, err);
  page *root = new_page(x, err); /* the root split: a new one above */
  if (!root) return 0;
  memcpy(all, mid, es);
  memset(all + es, 0, es);
  xb_put_le32(all + es, rightOffset);
  store(x, root->b, all, 1);
  x->root = root->offset;
  x->headerDirty = 1;
  release(root);
  return 1;
}

static int insert_entry(xb_ntx *x, const unsigned char *key, uint32_t recno, int32_t *err) {
  uint32_t found;
  if (x->info.unique) {
    xb_ntx_bound u = {key, x->info.keyLen, 0, 0, 0};
    int r = descend(x, &u, &found, err);
    if (r < 0) return 0;
    if (r > 0 && memcmp(xb_ntx_key(x), key, x->info.keyLen) == 0) return 1;
  }
  xb_ntx_bound t = {key, x->info.keyLen, recno, 1, 0};
  if (descend(x, &t, &found, err) < 0) return 0;
  /* The descent went down to a leaf; climbing back (on an ancestor's entry) left the path shorter: go down again
   * along the child before that entry to the leaf where the new one goes, at its end. */
  page *p = get(x, x->path[x->depth - 1].page, err);
  if (!p) return 0;
  uint32_t c = child(p->b, x->path[x->depth - 1].pos);
  release(p);
  if (c && !edge(x, c, 1, err)) return 0;
  uint32_t pos = x->path[x->depth - 1].pos;
  if (c) { /* edge() left pos on the last entry: the new one goes after it */
    p = get(x, x->path[x->depth - 1].page, err);
    if (!p) return 0;
    pos = count(p->b);
    release(p);
  }
  unsigned char e[XB_NTX_KEY_MAX + NTX_ENTRY_HEAD] = {0}; /* not in the scratch area: a split uses it */
  xb_put_le32(e + 4, recno);
  memcpy(e + NTX_ENTRY_HEAD, key, x->info.keyLen);
  ++x->changes;
  x->on = 0;
  return insert_at(x, x->depth - 1, pos, e, 0, err);
}

/* Removes entry `pos` of a page (its page offset goes with it; the next entry keeps its own). */
static void remove_entry(xb_ntx *x, page *p, uint32_t pos) {
  unsigned char *b = p->b;
  uint32_t n = count(b);
  uint16_t slot = xb_get_le16(b + 2 + 2 * pos);
  memmove(b + 2 + 2 * pos, b + 2 + 2 * (pos + 1), 2 * (size_t)(n - pos));
  xb_put_le16(b + 2 + 2 * n, slot);
  xb_put_le16(b, (uint16_t)(n - 1));
  touched(x, p);
}

/* After a removal at path level d: a page under half takes an entry from a sibling with more than half, or joins it
 * with the entry between them; up the path as the parent loses one. */
static int rebalance_up(xb_ntx *x, uint32_t d, int32_t *err) {
  size_t es = x->entrySize;
  for (; d > 0; --d) {
    page *p = get(x, x->path[d].page, err);
    if (!p) return 0;
    if (count(p->b) >= x->half) {
      release(p);
      return 1;
    }
    page *parent = get(x, x->path[d - 1].page, err);
    if (!parent) {
      release(p);
      return 0;
    }
    uint32_t ci = x->path[d - 1].pos, pn = count(parent->b);
    int useLeft = ci > 0;
    uint32_t si = useLeft ? ci - 1 : ci; /* the entry between the two pages in the parent */
    page *s = get(x, child(parent->b, useLeft ? ci - 1 : ci + 1), err);
    if (!s || ci > pn || (!useLeft && ci + 1 > pn)) {
      if (s) release(s);
      release(parent);
      release(p);
      if (s) *err = XB_FERR_FAILURE;
      return 0;
    }
    page *lp = useLeft ? s : p, *rp = useLeft ? p : s;
    unsigned char *all = x->scratch;
    uint32_t ln = load(x, lp->b, all);
    memcpy(all + ln * es + 4, entry(parent->b, si) + 4, es - 4); /* the separator, with the left's last page */
    uint32_t rn = load(x, rp->b, all + (ln + 1) * es);
    uint32_t total = ln + 1 + rn;
    if ((useLeft ? ln : rn) > x->half) { /* even shares, the separator between */
      uint32_t left = total / 2;
      store(x, lp->b, all, left);
      memcpy(entry(parent->b, si) + 4, all + left * es + 4, es - 4);
      store(x, rp->b, all + (left + 1) * es, total - left - 1);
      touched(x, lp);
      touched(x, rp);
      touched(x, parent);
      release(s);
      release(parent);
      release(p);
      return 1;
    }
    store(x, lp->b, all, total); /* joined into the left page; the right one is free */
    touched(x, lp);
    uint32_t leftOffset = lp->offset;
    free_page(x, rp);
    xb_put_le32(entry(parent->b, si + 1), leftOffset);
    remove_entry(x, parent, si);
    int emptyRoot = d - 1 == 0 && count(parent->b) == 0;
    if (emptyRoot) { /* the root lost its last entry: its only page is the root now */
      x->root = leftOffset;
      x->headerDirty = 1;
      free_page(x, parent);
    }
    release(s);
    release(parent);
    release(p);
    if (emptyRoot) return 1;
  }
  return 1;
}

static int delete_entry(xb_ntx *x, const unsigned char *key, uint32_t recno, int32_t *err) {
  uint32_t found;
  int r = find_entry(x, key, recno, &found, err);
  if (r < 0) return 0;
  if (r == 0 || found != recno || memcmp(xb_ntx_key(x), key, x->info.keyLen) != 0) return 1; /* not there */
  ++x->changes;
  x->on = 0;
  uint32_t d = x->depth - 1, pos = x->path[d].pos;
  page *p = get(x, x->path[d].page, err);
  if (!p) return 0;
  uint32_t c = child(p->b, pos);
  if (c) { /* a branch entry: the one before it, last in a leaf, takes its place */
    release(p);
    if (!edge(x, c, 1, err)) return 0;
    page *leaf = get(x, x->path[x->depth - 1].page, err);
    if (!leaf) return 0;
    p = get(x, x->path[d].page, err);
    if (!p) {
      release(leaf);
      return 0;
    }
    uint32_t last = count(leaf->b) - 1;
    memcpy(entry(p->b, pos) + 4, entry(leaf->b, last) + 4, x->entrySize - 4);
    touched(x, p);
    release(p);
    remove_entry(x, leaf, last);
    release(leaf);
    return rebalance_up(x, x->depth - 1, err);
  }
  remove_entry(x, p, pos);
  release(p);
  return rebalance_up(x, d, err);
}

/* Shared indexes. Every operation holds the index lock (a read lock to read, a write lock to change it) and first
 * reads the header's change counter: another program's (or area's) change since the last operation makes the pages
 * kept stale, and the cursor is placed again on its entry (gone: on the one after it, `displaced`). A change is
 * written, with the counter one higher, before the lock goes. An exclusive index skips all of it. */

/* Drops the pages kept (none is changed between operations of a shared index). */
static void forget_pages(xb_ntx *x) {
  for (uint32_t i = 0; i < NTX_CACHE; ++i) {
    page *p = &x->cache[i];
    if (p->offset) unhash(x, p);
    p->offset = 0;
    p->dirty = 0;
  }
}

static int unlock_index(xb_ntx *x, int ok, int32_t *err) {
  int32_t unlockErr = 0;
  if (!xb_shared_unlock(x->s, XB_DB_LOCK, 1, &unlockErr) && ok) {
    *err = unlockErr;
    ok = 0;
  }
  return ok;
}

static int begin(xb_ntx *x, int kind, int32_t *err) {
  if (!x->shared) return 1;
  if (!xb_shared_lock(x->s, XB_DB_LOCK, 1, kind, 1, err)) return 0;
  x->displaced = 0;
  unsigned char h[NTX_COUNTS];
  if (!xb_file_read_at(x->f, 0, h, sizeof h, err)) return unlock_index(x, 0, err);
  uint16_t version = xb_get_le16(h + NTX_VERSION);
  if (version == x->version && !x->stale) return 1;
  x->stale = 0;
  int64_t end = xb_platform_file_seek(x->f, 0, FS_END, err);
  if (end < 0) return unlock_index(x, 0, err);
  forget_pages(x);
  x->version = version;
  x->root = xb_get_le32(h + NTX_ROOT);
  x->freeList = xb_get_le32(h + NTX_FREE);
  x->end = end > UINT32_MAX ? UINT32_MAX / NTX_PAGE * NTX_PAGE : (uint32_t)(end / NTX_PAGE * NTX_PAGE);
  ++x->changes;
  if (!x->on) return 1;
  uint32_t was = x->onRecno, found;
  unsigned char key[XB_NTX_KEY_MAX];
  memcpy(key, x->onKey, x->info.keyLen);
  int r = find_entry(x, key, was, &found, err);
  if (r < 0) return unlock_index(x, 0, err);
  x->displaced = r == 0 || found != was || memcmp(x->onKey, key, x->info.keyLen) != 0;
  return 1;
}

/* Ends an operation that began (`ok`: it did not fail); `wrote`: it changed the index, which is written now. */
static int finish(xb_ntx *x, int ok, int wrote, int32_t *err) {
  if (!x->shared) return ok;
  if (ok && wrote) ok = xb_ntx_flush(x, err);
  if (!ok && wrote) { /* nothing half done is written later: the file is read again at the next operation */
    forget_pages(x);
    x->changed = x->headerDirty = 0;
    x->stale = 1;
  }
  return unlock_index(x, ok, err);
}

int xb_ntx_first(xb_ntx *x, uint32_t *recno, int32_t *err) {
  if (!begin(x, XB_LOCK_READ, err)) return -1;
  int r = go_first(x, recno, err);
  return finish(x, r >= 0, 0, err) ? r : -1;
}

int xb_ntx_last(xb_ntx *x, uint32_t *recno, int32_t *err) {
  if (!begin(x, XB_LOCK_READ, err)) return -1;
  int r = go_last(x, recno, err);
  return finish(x, r >= 0, 0, err) ? r : -1;
}

int xb_ntx_next(xb_ntx *x, uint32_t *recno, int32_t *err) {
  if (!begin(x, XB_LOCK_READ, err)) return -1;
  int r;
  if (!x->displaced) r = go_next(x, recno, err);
  else if (x->on) r = (*recno = x->onRecno, 1); /* the entry after the one that went */
  else r = 0;
  return finish(x, r >= 0, 0, err) ? r : -1;
}

int xb_ntx_prev(xb_ntx *x, uint32_t *recno, int32_t *err) {
  if (!begin(x, XB_LOCK_READ, err)) return -1;
  int r = x->displaced && !x->on ? go_last(x, recno, err) : go_prev(x, recno, err);
  return finish(x, r >= 0, 0, err) ? r : -1;
}

int xb_ntx_seek(xb_ntx *x, const unsigned char *key, size_t len, int last, uint32_t *recno, int32_t *err) {
  if (!begin(x, XB_LOCK_READ, err)) return -1;
  int r = seek_key(x, key, len, last, recno, err);
  return finish(x, r >= 0, 0, err) ? r : -1;
}

int xb_ntx_find(xb_ntx *x, const unsigned char *key, uint32_t recno, uint32_t *found, int32_t *err) {
  if (!begin(x, XB_LOCK_READ, err)) return -1;
  int r = find_entry(x, key, recno, found, err);
  return finish(x, r >= 0, 0, err) ? r : -1;
}

int xb_ntx_count(xb_ntx *x, const xb_ntx_bound *lo, const xb_ntx_bound *hi, uint64_t *n, int32_t *err) {
  if (!begin(x, XB_LOCK_READ, err)) return 0;
  return finish(x, count_range(x, lo, hi, n, err), 0, err);
}

int xb_ntx_count_before(xb_ntx *x, const xb_ntx_bound *lo, uint64_t *n, int32_t *err) {
  if (!begin(x, XB_LOCK_READ, err)) return 0;
  return finish(x, count_to_cursor(x, lo, n, err), 0, err);
}

int xb_ntx_select(xb_ntx *x, const xb_ntx_bound *lo, uint64_t k, uint32_t *recno, int32_t *err) {
  if (!begin(x, XB_LOCK_READ, err)) return -1;
  int r = select_entry(x, lo, k, recno, err);
  return finish(x, r >= 0, 0, err) ? r : -1;
}

int xb_ntx_replace(xb_ntx *x, const unsigned char *old, const unsigned char *key, uint32_t recno, int32_t *err) {
  if (!begin(x, XB_LOCK_WRITE, err)) return 0;
  int ok = (!old || delete_entry(x, old, recno, err)) && (!key || insert_entry(x, key, recno, err));
  return finish(x, ok, 1, err);
}
