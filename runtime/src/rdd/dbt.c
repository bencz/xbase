/* DBT memo files (rdd/dbt.h). Every binary field is little-endian (core/bytes.h):
 *
 *   block 0, the header: 0-3 the first free block; the rest of its 512 bytes zero;
 *   each memo from the start of a block: its text, then two 0x1A; the next memo starts at the next free block (the
 *     bytes between are zero), and the file ends after the last memo's end marks.
 *
 * A memo is never rewritten in place: a new value is a new memo after the last, and PACK drops the old ones. The
 * first free block is never taken below the end of the file, whatever the header says: a memo written there would
 * overwrite one in use. */
#include "rdd/dbt.h"

#include "core/bytes.h"
#include "core/file.h"

#include <stdlib.h>
#include <string.h>

#define DBT_BLOCK 512
#define DBT_END 0x1A
#define DBT_COPY 65536 /* bytes moved at a time when compacting */

static int64_t block_pos(uint32_t block) { return (int64_t)block * DBT_BLOCK; }

static int write_next(xb_dbt *m, int32_t *err) {
  unsigned char word[4];
  xb_put_le32(word, m->next);
  return xb_file_write_at(m->f, 0, word, sizeof word, err);
}

int xb_dbt_create(const char *path, int32_t *err) {
  unsigned char header[DBT_BLOCK] = {0};
  xb_put_le32(header, 1);
  xb_file f;
  if (!xb_platform_file_create(path, FC_NORMAL, &f, err)) return 0;
  int ok = xb_platform_file_write(f, header, sizeof header, err);
  int32_t closeErr = 0;
  if (!xb_platform_file_close(f, &closeErr) && ok) {
    *err = closeErr;
    ok = 0;
  }
  return ok;
}

int xb_dbt_open(xb_dbt *m, const char *path, int readonly, int32_t *err) {
  if (!xb_platform_file_open(path, readonly ? FO_READ : FO_READWRITE, &m->f, err)) return 0;
  unsigned char word[4];
  int64_t end = -1;
  if (!xb_file_read_at(m->f, 0, word, sizeof word, err) || (end = xb_platform_file_seek(m->f, 0, FS_END, err)) < 0) {
    int32_t ignored;
    xb_platform_file_close(m->f, &ignored);
    return 0;
  }
  uint64_t used = ((uint64_t)end + DBT_BLOCK - 1) / DBT_BLOCK;
  m->next = xb_get_le32(word);
  if (m->next < used) m->next = used > UINT32_MAX ? UINT32_MAX : (uint32_t)used;
  return 1;
}

int xb_dbt_close(xb_dbt *m, int32_t *err) { return xb_platform_file_close(m->f, err); }

int xb_dbt_read(xb_dbt *m, uint32_t block, xb_item *r, int32_t *err) {
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

int xb_dbt_append(xb_dbt *m, const char *p, size_t n, uint32_t *block, int32_t *err) {
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
    if (!xb_file_write_at(m->f, end, zeros, (size_t)(at - end < DBT_BLOCK ? at - end : DBT_BLOCK), err)) return 0;
  if (!xb_file_write_at(m->f, at, p, n, err) || !xb_platform_file_write(m->f, ends, sizeof ends, err)) return 0;
  *block = m->next;
  m->next += (uint32_t)blocks;
  return write_next(m, err);
}

int xb_dbt_zap(xb_dbt *m, int32_t *err) {
  m->next = 1;
  return xb_platform_file_truncate(m->f, DBT_BLOCK, err) && write_next(m, err);
}

int xb_dbt_move_down(xb_dbt *m, uint32_t from, int *moved, int32_t *err) {
  *moved = 0;
  int64_t src = block_pos(from), end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  int64_t len = end > src ? end - src : 0;
  if (from <= 1 || len > src - DBT_BLOCK) return 1; /* nothing before, or no room: the memos stay where they are */
  unsigned char *buf = malloc(DBT_COPY);
  if (!buf) xb_gc_fatal("out of memory");
  int ok = 1;
  for (int64_t done = 0; ok && done < len;) {
    size_t n = (size_t)(len - done < DBT_COPY ? len - done : DBT_COPY);
    ok = xb_file_read_at(m->f, src + done, buf, n, err) && xb_file_write_at(m->f, DBT_BLOCK + done, buf, n, err);
    done += (int64_t)n;
  }
  free(buf);
  *moved = ok;
  return ok;
}

int xb_dbt_cut(xb_dbt *m, uint32_t from, int32_t *err) {
  int64_t end = xb_platform_file_seek(m->f, 0, FS_END, err);
  if (end < 0) return 0;
  int64_t len = end > block_pos(from) ? end - block_pos(from) : 0;
  m->next = m->next - from + 1;
  return xb_platform_file_truncate(m->f, DBT_BLOCK + len, err) && write_next(m, err);
}
