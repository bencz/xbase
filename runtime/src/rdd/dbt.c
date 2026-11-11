/* DBT memo files (rdd/memo.h). Every binary field is little-endian (core/bytes.h):
 *
 *   block 0, the header: 0-3 the first free block; the rest of its 512 bytes zero;
 *   each memo from the start of a block: its text, then two 0x1A; the next memo starts at the next free block (the
 *     bytes between are zero), and the file ends after the last memo's end marks.
 *
 * A memo is never rewritten in place: a new value is a new memo after the last, and PACK drops the old ones. The
 * first free block is never taken below the end of the file, whatever the header says: a memo written there would
 * overwrite one in use. */
#include "rdd/memo.h"

#include "core/bytes.h"
#include "core/file.h"
#include "core/shared_file.h"

#include <stdlib.h>
#include <string.h>

#define DBT_BLOCK 512
#define DBT_END 0x1A
#define DBT_LOCK 0 /* the byte locked while a memo is added to a shared file */

struct xb_memo {
  xb_shared *s;
  xb_file f;     /* s's handle */
  uint32_t next; /* the first free block, kept in the file's header (read again under the lock when shared) */
};

static int64_t block_pos(uint32_t block) { return (int64_t)block * DBT_BLOCK; }

static int write_next(xb_memo *m, int32_t *err) {
  unsigned char word[4];
  xb_put_le32(word, m->next);
  return xb_platform_file_write_at(m->f, 0, word, sizeof word, err);
}

static int dbt_create(const char *path, int32_t *err) {
  unsigned char header[DBT_BLOCK] = {0};
  xb_put_le32(header, 1);
  xb_shared *s;
  if (!xb_shared_open(path, 0, 1, 1, &s, err)) return 0;
  int ok = xb_platform_file_write_at(xb_shared_handle(s), 0, header, sizeof header, err);
  int32_t closeErr = 0;
  if (!xb_shared_close(s, &closeErr) && ok) {
    *err = closeErr;
    ok = 0;
  }
  return ok;
}

/* The first free block from the header, never below the end of the file. */
static int read_next(xb_memo *m, int32_t *err) {
  unsigned char word[4];
  int64_t end = -1;
  if (!xb_file_read_at(m->f, 0, word, sizeof word, err) || (end = xb_platform_file_seek(m->f, 0, FS_END, err)) < 0)
    return 0;
  uint64_t used = ((uint64_t)end + DBT_BLOCK - 1) / DBT_BLOCK;
  m->next = xb_get_le32(word);
  if (m->next < used) m->next = used > UINT32_MAX ? UINT32_MAX : (uint32_t)used;
  return 1;
}

static int dbt_open(const char *path, int readonly, int exclusive, xb_memo **out, int32_t *err) {
  xb_memo *m = calloc(1, sizeof *m);
  if (!m) xb_gc_fatal("out of memory");
  if (xb_shared_open(path, readonly, exclusive, 0, &m->s, err)) {
    m->f = xb_shared_handle(m->s);
    if (read_next(m, err)) {
      *out = m;
      return 1;
    }
    int32_t ignored;
    xb_shared_close(m->s, &ignored);
  }
  free(m);
  return 0;
}

static int dbt_close(xb_memo *m, int32_t *err) {
  int ok = xb_shared_close(m->s, err);
  free(m);
  return ok;
}

static int dbt_read(xb_memo *m, uint32_t block, xb_item *r, int32_t *err) {
  size_t len = 0, cap = DBT_BLOCK;
  char *text = malloc(cap);
  if (!text) xb_gc_fatal("out of memory");
  int ok = block == 0 || xb_platform_file_seek(m->f, block_pos(block), FS_SET, err) == block_pos(block);
  for (int ended = block == 0; ok && !ended;) {
    if (cap - len < DBT_BLOCK) {
      if (cap > SIZE_MAX / 2) { /* no end mark in more than the memory can hold */
        *err = XB_FERR_FAILURE;
        ok = 0;
        break;
      }
      cap *= 2;
      char *grown = realloc(text, cap);
      if (!grown) xb_gc_fatal("out of memory");
      text = grown;
    }
    int64_t got = xb_file_read_up_to(m->f, text + len, DBT_BLOCK, err);
    if (got < 0) {
      ok = 0;
      break;
    }
    const char *end = memchr(text + len, DBT_END, (size_t)got);
    len = end ? (size_t)(end - text) : len + (size_t)got;
    ended = end || got < DBT_BLOCK;
  }
  if (ok) xb_set_str(r, xb_str_new(text, len));
  free(text);
  return ok;
}

/* A new memo after the last one, the file to itself (exclusive, or under its lock). */
static int append(xb_memo *m, const char *p, size_t n, uint32_t *block, int32_t *err) {
  static const unsigned char ends[2] = {DBT_END, DBT_END};
  uint64_t blocks = ((uint64_t)n + sizeof ends + DBT_BLOCK - 1) / DBT_BLOCK;
  if (blocks > UINT32_MAX - m->next) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  int64_t at = block_pos(m->next), end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  static const unsigned char zeros[DBT_BLOCK] = {0};
  for (; end < at; end += DBT_BLOCK) /* the rest of the last memo's block */
    if (!xb_platform_file_write_at(m->f, end, zeros, (size_t)(at - end < DBT_BLOCK ? at - end : DBT_BLOCK), err))
      return 0;
  if (!xb_platform_file_write_at(m->f, at, p, n, err) ||
      !xb_platform_file_write_at(m->f, at + (int64_t)n, ends, sizeof ends, err))
    return 0;
  *block = m->next;
  m->next += (uint32_t)blocks;
  return write_next(m, err);
}

/* A memo is never rewritten in place: the old one stays until PACK. */
static int dbt_write(xb_memo *m, uint32_t old, const char *p, size_t n, uint32_t *block, int32_t *err) {
  (void)old;
  *block = 0;
  if (!n) return 1;
  if (xb_shared_exclusive(m->s)) return append(m, p, n, block, err);
  if (!xb_shared_lock(m->s, DBT_LOCK, 1, XB_LOCK_WRITE, 1, err)) return 0;
  int ok = read_next(m, err) && append(m, p, n, block, err); /* another program may have added memos */
  int32_t unlockErr = 0;
  if (!xb_shared_unlock(m->s, DBT_LOCK, 1, &unlockErr) && ok) {
    *err = unlockErr;
    ok = 0;
  }
  return ok;
}

static int dbt_zap(xb_memo *m, int32_t *err) {
  m->next = 1;
  return xb_platform_file_truncate(m->f, DBT_BLOCK, err) && write_next(m, err);
}

static int dbt_pack_begin(xb_memo *m, uint32_t *from, int32_t *err) {
  (void)err;
  *from = m->next;
  return 1;
}

static int dbt_pack_copy(xb_memo *m, uint32_t block, uint32_t *copy, int32_t *err) {
  xb_item text;
  *copy = 0;
  if (!dbt_read(m, block, &text, err)) return 0; /* unrooted: nothing allocates while it is used */
  return !text.v.s->len || append(m, text.v.s->data, text.v.s->len, copy, err);
}

static int dbt_pack_move(xb_memo *m, uint32_t from, uint32_t *shift, int32_t *err) {
  *shift = 0;
  int64_t src = block_pos(from), end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  int64_t len = end > src ? end - src : 0;
  if (from <= 1 || len > src - DBT_BLOCK) return 1; /* nothing before, or no room: the memos stay where they are */
  if (!xb_file_copy_within(m->f, src, DBT_BLOCK, len, err)) return 0;
  *shift = from - 1;
  return 1;
}

static int dbt_pack_end(xb_memo *m, uint32_t from, uint32_t shift, int32_t *err) {
  if (!shift) return 1;
  int64_t end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  int64_t len = end > block_pos(from) ? end - block_pos(from) : 0;
  m->next -= shift;
  return xb_platform_file_truncate(m->f, DBT_BLOCK + len, err) && write_next(m, err);
}

const xb_memo_format xb_memo_dbt = {
    .ext = ".dbt",
    .version = 0x83,
    .create = dbt_create,
    .open = dbt_open,
    .close = dbt_close,
    .read = dbt_read,
    .write = dbt_write,
    .zap = dbt_zap,
    .pack_begin = dbt_pack_begin,
    .pack_copy = dbt_pack_copy,
    .pack_move = dbt_pack_move,
    .pack_end = dbt_pack_end,
};
