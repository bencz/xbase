/* FPT memo files (FoxPro, with the FlexFile free list as Clipper's FlexFile and the reference keep it; rdd/memo.h).
 *
 *   the header, 1024 bytes: 0-3 the first free block (big-endian), 4-7 the block size (big-endian; FoxPro reads
 *     6-7), 8-15 a writer's signature (zero here), 0x200-0x209 "FlexFile3" and 3, then little-endian: 0x20C the byte
 *     offset of the free list's reversed page, 0x210 of its page (0: none), 0x214 a counter of changes; zeros else;
 *   a memo from the start of a block: its type (1: text) and length, big-endian, then its bytes; the byte 0xAF at the
 *     end of its last block unless the memo fills it; the rest of the block is not written;
 *   the free list's page: type 1000 and payload length 1010 (big-endian), then the payload, 0xAD where unused: the
 *     item count c as 4c + 3 (little-endian 16 bits), then per item its byte offset and byte length (little-endian);
 *     the reversed page holds the same items length first; at most FPT_ITEMS items;
 *   a free area begins with type 1001 and its length less 8 (big-endian) when the list marks it changed.
 *
 * Every write is one operation: the old memo's blocks go back (to the end of the file, or into the list, joined with
 * the free areas beside them), then the new memo takes the smallest free area it fits (exactly: the area goes; the
 * marks of changed areas then stay with their places in the list, as the reference keeps them), or the end; the list,
 * the free areas' headers and the header are written; the file is cut when it ends sooner. A file without the
 * FlexFile mark keeps no list: what a write frees serves that write only, and no 0xAF is written. Shared, a write
 * holds the lock of byte 0 and starts from the header and list as they are on disk. */
#include "rdd/memo.h"

#include "core/bytes.h"
#include "core/file.h"
#include "core/shared_file.h"

#include <stdlib.h>
#include <string.h>

#define FPT_HEADER 1024
#define FPT_HALF 512          /* the header part a file without the FlexFile mark keeps */
#define FPT_BLOCK 64          /* the block size of a new file */
#define FPT_TEXT 1
#define FPT_PAGE 1000         /* the type of the free list's pages */
#define FPT_FREE 1001         /* the type of a free area */
#define FPT_PAYLOAD 1010      /* bytes of a free list page after its type and length */
#define FPT_ITEMS 126         /* items a page holds: (FPT_PAYLOAD - 2) / 8 */
#define FPT_ITEM_HEAD 8       /* type and length before a memo's bytes, or a page's */
#define FPT_TRAILER 0xAF
#define FPT_UNUSED 0xAD       /* the unused bytes of a free list page */
#define FPT_LOCK 0            /* the byte locked while the file changes (shared) */
#define FPT_FLEX 0x200        /* the FlexFile mark, then the list's page offsets and the counter */
#define FPT_REV 0x20C
#define FPT_DIR 0x210
#define FPT_COUNTER 0x214
#define FPT_FLEX_BLOCK 0x21C  /* FlexFile's own block size (little-endian 16 bits), when the header's is 0 */
#define FPT_SIGNATURE 8       /* a writer's signature; one there marks a free list of another kind */
#define FPT_MAX_POS UINT32_MAX /* the list's pages hold byte offsets in 32 bits: a FlexFile file stays below 4 GB */

static const char flex_mark[10] = {'F', 'l', 'e', 'x', 'F', 'i', 'l', 'e', '3', 3};
static const char six_mark[7] = {'S', 'I', 'x', 'M', 'e', 'm', 'o'};

typedef struct area {
  uint32_t offset, size; /* in blocks */
} area;

struct xb_memo {
  xb_shared *s;
  xb_file f;
  int exclusive;
  int flex;                            /* the file has the FlexFile mark: it keeps a free list */
  uint32_t bs;                         /* the block size */
  unsigned char header[FPT_HEADER];    /* as read (shared: again for each write) */
  uint32_t next;                       /* the first block past the allocated ones */
  area items[FPT_ITEMS + 1];           /* the free list, by size (equal sizes in the order they came) */
  unsigned char marks[FPT_ITEMS + 1];  /* per place in the list: the area there needs its header written */
  uint32_t count;
  int changed;                         /* the list changed in this write */
};

static uint32_t blocks_for(const xb_memo *m, uint64_t bytes) { return (uint32_t)((bytes + m->bs - 1) / m->bs); }

static int64_t pos_of(const xb_memo *m, uint32_t block) { return (int64_t)block * m->bs; }

/* The header's length: FoxPro's half without the FlexFile part. */
static uint32_t header_len(int flex) { return flex ? FPT_HEADER : FPT_HALF; }

/* The first block a memo can take. */
static uint32_t first_block(const xb_memo *m) { return blocks_for(m, header_len(m->flex)); }

/* A new file's header (`flex`: with the FlexFile mark) and its length: the header, then zeros up to the first block
 * when that is past it (with the mark, as the reference makes it: measured from the header's end, so it stops short
 * when blocks are over 2048 bytes). */
static int64_t new_header(uint32_t bs, int flex, unsigned char *h) {
  uint32_t len = header_len(flex), next = (len + bs - 1) / bs;
  memset(h, 0, FPT_HEADER);
  xb_put_be32(h, next);
  xb_put_be32(h + 4, bs);
  if (!flex) return (int64_t)next * bs;
  memcpy(h + FPT_FLEX, flex_mark, sizeof flex_mark);
  int64_t rest = (int64_t)next * bs - FPT_HEADER;
  return rest > FPT_HEADER ? rest : FPT_HEADER;
}

static int write_new(xb_file f, uint32_t bs, int flex, int32_t *err) {
  unsigned char h[FPT_HEADER];
  int64_t size = new_header(bs, flex, h);
  return xb_platform_file_write_at(f, 0, h, header_len(flex), err) && xb_platform_file_truncate(f, size, err);
}

static int fpt_create(const char *path, int32_t *err) {
  xb_shared *s;
  if (!xb_shared_open(path, 0, 1, 1, &s, err)) return 0;
  int ok = write_new(xb_shared_handle(s), FPT_BLOCK, 1, err);
  int32_t closeErr = 0;
  if (!xb_shared_close(s, &closeErr) && ok) {
    *err = closeErr;
    ok = 0;
  }
  return ok;
}

/* Stable by size: equal sizes keep their order, the marks go with their areas. */
static void sort_items(xb_memo *m) {
  for (uint32_t i = 1; i < m->count; ++i) {
    area a = m->items[i];
    unsigned char mark = m->marks[i];
    uint32_t j = i;
    for (; j > 0 && m->items[j - 1].size > a.size; --j) {
      m->items[j] = m->items[j - 1];
      m->marks[j] = m->marks[j - 1];
    }
    m->items[j] = a;
    m->marks[j] = mark;
  }
}

/* The header: 1024 bytes, or FoxPro's 512 in a file without the FlexFile part. */
static int read_header(xb_memo *m, int32_t *err) {
  memset(m->header, 0, FPT_HEADER);
  int64_t end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  return xb_file_read_at(m->f, 0, m->header, end < FPT_HEADER ? FPT_HALF : FPT_HEADER, err);
}

/* The header and the free list from the file. */
static int load(xb_memo *m, int32_t *err) {
  if (!read_header(m, err)) return 0;
  m->next = xb_get_be32(m->header);
  m->count = 0;
  m->changed = 0;
  memset(m->marks, 0, sizeof m->marks);
  uint32_t dir = m->flex ? xb_get_le32(m->header + FPT_DIR) : 0;
  if (!dir) return 1;
  unsigned char page[FPT_ITEM_HEAD + FPT_PAYLOAD];
  if (!xb_file_read_at(m->f, dir, page, sizeof page, err)) return 0;
  uint32_t n = ((uint32_t)xb_get_le16(page + FPT_ITEM_HEAD) - 3) >> 2;
  if (xb_get_be32(page) != FPT_PAGE || xb_get_be32(page + 4) < FPT_PAYLOAD || n > FPT_ITEMS) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  for (uint32_t i = 0; i < n; ++i) { /* an area that is not one (a damaged list) is left out */
    const unsigned char *p = page + FPT_ITEM_HEAD + 2 + 8 * i;
    area a = {xb_get_le32(p) / m->bs, xb_get_le32(p + 4) / m->bs};
    if (a.size && a.offset >= first_block(m) && a.offset < m->next && a.size <= m->next - a.offset)
      m->items[m->count++] = a;
  }
  sort_items(m);
  return 1;
}

static int fpt_open(const char *path, int readonly, int exclusive, xb_memo **out, int32_t *err) {
  xb_memo *m = calloc(1, sizeof *m);
  if (!m) xb_gc_fatal("out of memory");
  if (!xb_shared_open(path, readonly, exclusive, 0, &m->s, err)) {
    free(m);
    return 0;
  }
  m->f = xb_shared_handle(m->s);
  m->exclusive = exclusive;
  int ok = xb_shared_lock(m->s, FPT_LOCK, 1, XB_LOCK_READ, 1, err); /* a header another program is not writing */
  if (ok) {
    ok = read_header(m, err);
    if (ok) {
      m->flex = memcmp(m->header + FPT_FLEX, flex_mark, sizeof flex_mark) == 0 &&
                memcmp(m->header + FPT_SIGNATURE, six_mark, sizeof six_mark) != 0; /* another kind of list there */
      m->bs = xb_get_be32(m->header + 4);
      if (m->bs > UINT16_MAX + 1u && (m->bs & UINT16_MAX)) m->bs &= UINT16_MAX; /* 16 bits a writer put in 32 */
      if (m->flex && !m->bs) m->bs = xb_get_le16(m->header + FPT_FLEX_BLOCK);
      if (!m->bs) {
        *err = XB_FERR_FAILURE;
        ok = 0;
      }
      ok = ok && load(m, err);
    }
    int32_t ignored;
    xb_shared_unlock(m->s, FPT_LOCK, 1, &ignored);
  }
  if (!ok) {
    int32_t ignored;
    xb_shared_close(m->s, &ignored);
    free(m);
    return 0;
  }
  *out = m;
  return 1;
}

static int fpt_close(xb_memo *m, int32_t *err) {
  int ok = xb_shared_close(m->s, err);
  free(m);
  return ok;
}

static int fpt_read(xb_memo *m, uint32_t block, xb_item *r, int32_t *err) {
  if (!block) {
    xb_set_str(r, xb_str_empty());
    return 1;
  }
  unsigned char head[FPT_ITEM_HEAD];
  if (!xb_file_read_at(m->f, pos_of(m, block), head, sizeof head, err)) return 0;
  uint32_t type = xb_get_be32(head), len = xb_get_be32(head + 4);
  if (type > FPT_TEXT) { /* values of other kinds the format holds (arrays, numbers, ...): none here */
    xb_set_nil(r);
    return 1;
  }
  int64_t end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  if ((int64_t)len > end - pos_of(m, block) - FPT_ITEM_HEAD) { /* a length past the file: damaged */
    *err = XB_FERR_FAILURE;
    return 0;
  }
  char *text = malloc(len ? len : 1);
  if (!text) xb_gc_fatal("out of memory");
  int ok = xb_file_read_at(m->f, pos_of(m, block) + FPT_ITEM_HEAD, text, len, err);
  if (ok) xb_set_str(r, xb_str_new(text, len));
  free(text);
  return ok;
}

/* Joins free areas that touch and gives back to the end those that end there, until none does; then drops the
 * emptied places (their marks go with the others). */
static void coalesce(xb_memo *m) {
  for (int again = 1; again;) {
    again = 0;
    for (uint32_t i = 0; i < m->count && !again; ++i) {
      area *a = &m->items[i];
      if (!a->offset || !a->size) continue;
      if (a->offset + a->size == m->next) {
        m->next -= a->size;
        a->offset = a->size = 0;
        again = m->changed = 1;
        break;
      }
      for (uint32_t j = i + 1; j < m->count; ++j)
        if (m->items[j].offset == a->offset + a->size && m->items[j].size) {
          a->size += m->items[j].size;
          m->marks[i] = 1;
          m->items[j].offset = m->items[j].size = 0;
          again = m->changed = 1;
          break;
        }
    }
  }
  uint32_t kept = 0;
  for (uint32_t i = 0; i < m->count; ++i)
    if (m->items[i].offset && m->items[i].size) {
      m->items[kept] = m->items[i];
      m->marks[kept++] = m->marks[i];
    }
  for (uint32_t i = kept; i < m->count; ++i) m->marks[i] = 0;
  m->count = kept;
}

/* `k` blocks at `p` are free. */
static void release(xb_memo *m, uint32_t p, uint32_t k) {
  if (p + k == m->next) {
    m->next -= k;
    coalesce(m);
    return;
  }
  m->changed = 1;
  uint32_t i = 0;
  for (; i < m->count; ++i) {
    area *a = &m->items[i];
    if (a->offset + a->size == p) {
      a->size += k;
      break;
    }
    if (a->offset == p + k) {
      a->offset = p;
      a->size += k;
      break;
    }
  }
  if (i < m->count) {
    m->marks[i] = 1;
  } else if (m->count <= FPT_ITEMS) { /* one past FPT_ITEMS: the smallest goes when the list is written */
    m->items[m->count] = (area){p, k};
    m->marks[m->count++] = 1;
  }
  coalesce(m);
  sort_items(m);
}

/* The first block of `n` free ones: the smallest free area they fit (exactly: the area goes, the later ones move
 * up and the marks stay where they are), else the end. */
static uint32_t take(xb_memo *m, uint32_t n) {
  for (uint32_t i = 0; i < m->count; ++i) {
    area *a = &m->items[i];
    if (a->size < n) continue;
    uint32_t at = a->offset;
    m->changed = 1;
    if (a->size == n) {
      memmove(&m->items[i], &m->items[i + 1], (m->count - i - 1) * sizeof *m->items);
      m->marks[--m->count] = 0;
    } else {
      a->offset += n;
      a->size -= n;
      m->marks[i] = 1;
      sort_items(m);
    }
    return at;
  }
  uint32_t at = m->next;
  m->next += n;
  return at;
}

/* Writes one of the list's pages: `reversed` puts each area's length before its offset (and keeps 16 bits of its
 * block count there, as the reference does). */
static int write_page(xb_memo *m, uint32_t pos, int reversed, int32_t *err) {
  unsigned char page[FPT_ITEM_HEAD + FPT_PAYLOAD];
  memset(page, FPT_UNUSED, sizeof page);
  xb_put_be32(page, FPT_PAGE);
  xb_put_be32(page + 4, FPT_PAYLOAD);
  uint32_t n = m->count < FPT_ITEMS ? m->count : FPT_ITEMS, first = m->count - n; /* the smallest goes past them */
  xb_put_le16(page + FPT_ITEM_HEAD, (uint16_t)(n << 2 | 3));
  for (uint32_t i = 0; i < n; ++i) {
    const area *a = &m->items[first + i];
    unsigned char *p = page + FPT_ITEM_HEAD + 2 + 8 * i;
    uint32_t offset = a->offset * m->bs, size = (reversed ? (uint16_t)a->size : a->size) * m->bs;
    xb_put_le32(p, reversed ? size : offset);
    xb_put_le32(p + 4, reversed ? offset : size);
  }
  return xb_platform_file_write_at(m->f, pos, page, sizeof page, err);
}

/* The end of a write: the list's pages released or made and written, the changed free areas' headers, the header;
 * the file cut when it ends sooner than it did (`before`). */
static int finish(xb_memo *m, uint32_t before, int32_t *err) {
  unsigned char *h = m->header;
  if (!m->flex) { /* no list kept: what this write freed and did not take is lost */
    m->count = 0;
    m->changed = 0;
    memset(m->marks, 0, sizeof m->marks);
    xb_put_be32(h, m->next);
    return xb_platform_file_write_at(m->f, 0, h, FPT_HALF, err) &&
           (m->next >= before || xb_platform_file_truncate(m->f, pos_of(m, m->next), err));
  }
  xb_put_le32(h + FPT_COUNTER, xb_get_le32(h + FPT_COUNTER) + 1);
  uint32_t dir = xb_get_le32(h + FPT_DIR), rev = xb_get_le32(h + FPT_REV);
  if (!m->count && dir) { /* the pages go when they are the last blocks (measured from 1010 bytes each) */
    uint32_t pages = blocks_for(m, FPT_PAYLOAD) * (rev ? 2u : 1u);
    uint32_t start = (rev && rev < dir ? rev : dir) / m->bs;
    if (start + pages == m->next) {
      dir = rev = 0;
      m->next -= pages;
    }
  }
  if (m->count && !dir) {
    uint32_t pages = blocks_for(m, FPT_ITEM_HEAD + FPT_PAYLOAD);
    dir = (uint32_t)pos_of(m, take(m, pages));
    rev = (uint32_t)pos_of(m, take(m, pages));
  }
  if (dir && m->changed && (!write_page(m, dir, 0, err) || !write_page(m, rev, 1, err))) return 0;
  xb_put_le32(h + FPT_REV, rev);
  xb_put_le32(h + FPT_DIR, dir);
  for (uint32_t i = 0; m->changed && i < m->count; ++i) {
    const area *a = &m->items[i];
    uint64_t bytes = (uint64_t)a->size * m->bs;
    if (!m->marks[i] || bytes < FPT_ITEM_HEAD) continue;
    unsigned char head[FPT_ITEM_HEAD];
    xb_put_be32(head, FPT_FREE);
    xb_put_be32(head + 4, (uint32_t)(bytes - FPT_ITEM_HEAD));
    if (!xb_platform_file_write_at(m->f, pos_of(m, a->offset), head, sizeof head, err)) return 0;
  }
  memset(m->marks, 0, sizeof m->marks);
  if (m->count > FPT_ITEMS) { /* the one past what a page holds is forgotten */
    memmove(&m->items[0], &m->items[1], FPT_ITEMS * sizeof *m->items);
    m->count = FPT_ITEMS;
  }
  m->changed = 0;
  xb_put_be32(h, m->next);
  return xb_platform_file_write_at(m->f, 0, h, FPT_HEADER, err) &&
         (m->next >= before || xb_platform_file_truncate(m->f, pos_of(m, m->next), err));
}

/* fpt_write with the file to itself (exclusive, or under its lock). */
static int write_memo(xb_memo *m, uint32_t old, const char *p, size_t n, uint32_t *block, int32_t *err) {
  uint32_t before = m->next;
  if (old >= first_block(m) && old < m->next) { /* a memo elsewhere (a damaged record) is not freed */
    unsigned char head[FPT_ITEM_HEAD];
    if (!xb_file_read_at(m->f, pos_of(m, old), head, sizeof head, err)) return 0;
    uint32_t k = blocks_for(m, (uint64_t)xb_get_be32(head + 4) + FPT_ITEM_HEAD);
    if (xb_get_be32(head) <= FPT_TEXT && k <= m->next - old) release(m, old, k);
  }
  *block = 0;
  if (n) {
    uint64_t bytes = (uint64_t)n + FPT_ITEM_HEAD;
    uint32_t count = blocks_for(m, bytes);
    uint64_t pages = 2u * (uint64_t)blocks_for(m, FPT_ITEM_HEAD + FPT_PAYLOAD) * m->bs;
    if (bytes > UINT32_MAX || count > UINT32_MAX - m->next ||
        (m->flex && (uint64_t)pos_of(m, m->next) + (uint64_t)count * m->bs + pages > FPT_MAX_POS)) {
      *err = XB_FERR_DISK_FULL; /* past what the format addresses */
      return 0;
    }
    uint32_t at = take(m, count);
    unsigned char head[FPT_ITEM_HEAD];
    xb_put_be32(head, FPT_TEXT);
    xb_put_be32(head + 4, (uint32_t)n);
    int64_t pos = pos_of(m, at);
    static const unsigned char trailer = FPT_TRAILER;
    if (!xb_platform_file_write_at(m->f, pos, head, sizeof head, err) ||
        !xb_platform_file_write_at(m->f, pos + FPT_ITEM_HEAD, p, n, err) ||
        (m->flex && bytes % m->bs &&
         !xb_platform_file_write_at(m->f, pos_of(m, at + count) - 1, &trailer, 1, err)))
      return 0;
    *block = at;
  }
  return finish(m, before, err);
}

static int fpt_write(xb_memo *m, uint32_t old, const char *p, size_t n, uint32_t *block, int32_t *err) {
  *block = 0;
  if (!old && !n) return 1;
  if (m->exclusive) {
    if (write_memo(m, old, p, n, block, err)) return 1;
    int32_t ignored; /* what was freed or taken in memory and not written: the file's state again */
    load(m, &ignored);
    return 0;
  }
  if (!xb_shared_lock(m->s, FPT_LOCK, 1, XB_LOCK_WRITE, 1, err)) return 0;
  int ok = load(m, err) && write_memo(m, old, p, n, block, err); /* from what other programs wrote */
  int32_t unlockErr = 0;
  if (!xb_shared_unlock(m->s, FPT_LOCK, 1, &unlockErr) && ok) {
    *err = unlockErr;
    ok = 0;
  }
  return ok;
}

static int fpt_zap(xb_memo *m, int32_t *err) { return write_new(m->f, m->bs, m->flex, err) && load(m, err); }

/* The header's first free block on disk (PACK keeps it past every copy, so that a failure loses nothing). */
static int write_next(xb_memo *m, int32_t *err) {
  xb_put_be32(m->header, m->next);
  return xb_platform_file_write_at(m->f, 0, m->header, 4, err);
}

/* PACK: the copies go after the last block, each at the next block from the first one, as the reference packs them:
 * a memo's type, length and bytes, no 0xAF, so the blocks between are not filled. The old free list is dropped from
 * the start: a failure leaves its areas unused, never given to a new memo over a copy. */
static int fpt_pack_begin(xb_memo *m, uint32_t *from, int32_t *err) {
  int64_t end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  if (end > pos_of(m, m->next)) m->next = blocks_for(m, (uint64_t)end); /* past anything already there */
  *from = m->next;
  m->count = 0;
  m->changed = 0;
  if (m->flex) {
    xb_put_le32(m->header + FPT_REV, 0);
    xb_put_le32(m->header + FPT_DIR, 0);
    if (!xb_platform_file_write_at(m->f, FPT_REV, m->header + FPT_REV, 8, err)) return 0;
  }
  return write_next(m, err);
}

static int fpt_pack_copy(xb_memo *m, uint32_t block, uint32_t *copy, int32_t *err) {
  unsigned char head[FPT_ITEM_HEAD];
  if (!xb_file_read_at(m->f, pos_of(m, block), head, sizeof head, err)) return 0;
  uint64_t bytes = (uint64_t)xb_get_be32(head + 4) + FPT_ITEM_HEAD;
  int64_t end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  uint32_t at = blocks_for(m, (uint64_t)(end > pos_of(m, m->next) ? end : pos_of(m, m->next)));
  if (!xb_file_copy_within(m->f, pos_of(m, block), pos_of(m, at), (int64_t)bytes, err)) return 0;
  m->next = blocks_for(m, (uint64_t)pos_of(m, at) + bytes);
  *copy = at;
  return write_next(m, err);
}

static int fpt_pack_move(xb_memo *m, uint32_t from, uint32_t *shift, int32_t *err) {
  *shift = 0;
  uint32_t first = first_block(m);
  int64_t src = pos_of(m, from), end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  int64_t len = end > src ? end - src : 0;
  if (from <= first || len > src - pos_of(m, first)) return 1; /* no room before them: they stay */
  if (!xb_file_copy_within(m->f, src, pos_of(m, first), len, err)) return 0;
  *shift = from - first;
  return 1;
}

/* A fresh header, its first free block after the copies; the file cut after them. */
static int fpt_pack_end(xb_memo *m, uint32_t from, uint32_t shift, int32_t *err) {
  int64_t end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  uint32_t first = first_block(m);
  int64_t len = end > pos_of(m, from) ? end - pos_of(m, from) : 0;
  unsigned char h[FPT_HEADER];
  int64_t fresh = new_header(m->bs, m->flex, h); /* no memo kept: a new file's length */
  int64_t size = !shift ? end : len ? pos_of(m, first) + len : fresh;
  m->next = blocks_for(m, (uint64_t)size);
  xb_put_be32(h, m->next);
  memcpy(m->header, h, FPT_HEADER);
  m->count = 0;
  return xb_platform_file_write_at(m->f, 0, h, header_len(m->flex), err) &&
         xb_platform_file_truncate(m->f, size, err);
}

const xb_memo_format xb_memo_fpt = {
    .ext = ".fpt",
    .version = 0xF5,
    .create = fpt_create,
    .open = fpt_open,
    .close = fpt_close,
    .read = fpt_read,
    .write = fpt_write,
    .zap = fpt_zap,
    .pack_begin = fpt_pack_begin,
    .pack_copy = fpt_pack_copy,
    .pack_move = fpt_pack_move,
    .pack_end = fpt_pack_end,
};
