/* DBT memo files (rdd/memo.h). Every binary field is little-endian (core/bytes.h):
 *
 *   block 0, the header: 0-3 the first free block; the rest of its 512 bytes zero;
 *   each memo from the start of a block: its text, then two 0x1A; the next memo starts at the next free block (the
 *     bytes between are zero), and the file ends after the last memo's end marks.
 *
 * In a file open shared, a memo is never rewritten in place (another program may be reading it): a new value is a new
 * memo after the last, and PACK drops the old ones. In a file open exclusively, a new value whose text and end marks
 * fit the blocks of the memo it replaces (its room: the blocks up to the one holding its end marks) is written over
 * it, the blocks it no longer needs given back to a list of free runs of blocks kept while the file is open (sorted,
 * neighbors joined); one that does not fit gives the whole room back and goes in the first run that holds it, else
 * after the last memo. So memos changed
 * again and again keep the file near the size of what they hold, as PACK would leave it. The first free block is never
 * taken below the end of the file, whatever the header says: a memo written there would overwrite one in use. */
#include "rdd/memo.h"

#include "core/bytes.h"
#include "core/file.h"
#include "core/shared_file.h"

#include <stdlib.h>
#include <string.h>

#define DBT_BLOCK 512
#define DBT_END 0x1A
#define DBT_LOCK 0 /* the byte locked while a memo is added to a shared file */

typedef struct run {
  uint32_t block, count;
} run;

struct xb_memo {
  xb_shared *s;
  xb_file f;     /* s's handle */
  uint32_t next; /* the first free block, kept in the file's header (read again under the lock when shared) */
  run *runs;     /* free runs of blocks before `next`, by block (exclusive only) */
  size_t nruns, capRuns;
};

/* Gives back `count` blocks at `block`, joined to the runs next to them. */
static void give_back(xb_memo *m, uint32_t block, uint32_t count) {
  size_t k = 0;
  while (k < m->nruns && m->runs[k].block < block) ++k;
  int overlaps = (k > 0 && m->runs[k - 1].block + m->runs[k - 1].count > block) ||
                 (k < m->nruns && block + count > m->runs[k].block);
  if (overlaps) return; /* free already (two records on one memo: a damaged file): never handed out twice */
  int before = k > 0 && m->runs[k - 1].block + m->runs[k - 1].count == block;
  int after = k < m->nruns && block + count == m->runs[k].block;
  if (before && after) {
    m->runs[k - 1].count += count + m->runs[k].count;
    memmove(&m->runs[k], &m->runs[k + 1], (m->nruns - k - 1) * sizeof *m->runs);
    --m->nruns;
  } else if (before) {
    m->runs[k - 1].count += count;
  } else if (after) {
    m->runs[k].block = block;
    m->runs[k].count += count;
  } else {
    if (m->nruns == m->capRuns) {
      size_t cap = m->capRuns ? m->capRuns * 2 : 16;
      run *grown = realloc(m->runs, cap * sizeof *grown);
      if (!grown) xb_gc_fatal("out of memory");
      m->runs = grown;
      m->capRuns = cap;
    }
    memmove(&m->runs[k + 1], &m->runs[k], (m->nruns - k) * sizeof *m->runs);
    m->runs[k] = (run){block, count};
    ++m->nruns;
  }
}

/* The first block of the first free run of at least `count` blocks, taken from it: 0 when none is. */
static uint32_t take(xb_memo *m, uint32_t count) {
  for (size_t k = 0; k < m->nruns; ++k) {
    if (m->runs[k].count < count) continue;
    uint32_t block = m->runs[k].block;
    m->runs[k].block += count;
    m->runs[k].count -= count;
    if (!m->runs[k].count) {
      memmove(&m->runs[k], &m->runs[k + 1], (m->nruns - k - 1) * sizeof *m->runs);
      --m->nruns;
    }
    return block;
  }
  return 0;
}

static void forget_runs(xb_memo *m) { m->nruns = 0; }

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
  free(m->runs);
  free(m);
  return ok;
}

/* The text of the memo at `block` (the bytes before its first end mark, or before the end of the file), in `*text`
 * (malloc'd: not the collector's, so that no collection can move or free it while files are read and written), read
 * in chunks that double from a few blocks: a short memo costs one read, a long one a few. */
static int read_text(xb_memo *m, uint32_t block, char **text, size_t *len, int32_t *err) {
  size_t cap = 8 * DBT_BLOCK, chunk = cap;
  *len = 0;
  *text = malloc(cap);
  if (!*text) xb_gc_fatal("out of memory");
  if (block == 0) return 1;
  for (int64_t pos = block_pos(block);;) {
    while (cap - *len < chunk) {
      if (cap > SIZE_MAX / 2) { /* no end mark in more than the memory can hold */
        *err = XB_FERR_FAILURE;
        free(*text);
        return 0;
      }
      cap *= 2;
      char *grown = realloc(*text, cap);
      if (!grown) xb_gc_fatal("out of memory");
      *text = grown;
    }
    int64_t got = xb_file_read_up_to_at(m->f, pos, *text + *len, chunk, err);
    if (got < 0) {
      free(*text);
      return 0;
    }
    const char *end = memchr(*text + *len, DBT_END, (size_t)got);
    if (end) {
      *len = (size_t)(end - *text);
      return 1;
    }
    *len += (size_t)got;
    if ((size_t)got < chunk) return 1;
    pos += got;
    if (chunk < (size_t)1 << 20) chunk *= 2;
  }
}

static int dbt_read(xb_memo *m, uint32_t block, xb_item *r, int32_t *err) {
  char *text;
  size_t len;
  if (!read_text(m, block, &text, &len, err)) return 0;
  xb_set_str(r, xb_str_new(text, len));
  free(text);
  return 1;
}

/* Writes the text and its end marks at `block`. */
static int put(xb_memo *m, uint32_t block, const char *p, size_t n, int32_t *err) {
  static const unsigned char ends[2] = {DBT_END, DBT_END};
  return xb_platform_file_write_at(m->f, block_pos(block), p, n, err) &&
         xb_platform_file_write_at(m->f, block_pos(block) + (int64_t)n, ends, sizeof ends, err);
}

/* The blocks a memo of `n` bytes takes, with its two end marks. */
static uint64_t blocks_for(size_t n) { return ((uint64_t)n + 2 + DBT_BLOCK - 1) / DBT_BLOCK; }

/* A new memo after the last one, the file to itself (exclusive, or under its lock). */
static int append(xb_memo *m, const char *p, size_t n, uint32_t *block, int32_t *err) {
  uint64_t blocks = blocks_for(n);
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
  if (!put(m, m->next, p, n, err)) return 0;
  *block = m->next;
  m->next += (uint32_t)blocks;
  return write_next(m, err);
}

/* The room of the memo at `old` (see the top of the file): the blocks up to the one holding its first end mark, and the
 * next one when its second end mark starts it (a text that fills a block but one byte: no memo starts with an end
 * mark), never past the first free block. */
static int room_of(xb_memo *m, uint32_t old, uint32_t *room, int32_t *err) {
  char *text;
  size_t len;
  if (!read_text(m, old, &text, &len, err)) return 0;
  free(text);
  uint64_t blocks = (uint64_t)len / DBT_BLOCK + 1;
  unsigned char second = 0;
  if (len % DBT_BLOCK == DBT_BLOCK - 1 &&
      xb_file_read_up_to_at(m->f, block_pos(old) + (int64_t)len + 1, &second, 1, err) < 0)
    return 0;
  if (second == DBT_END) ++blocks;
  *room = blocks > m->next - old ? m->next - old : (uint32_t)blocks;
  return 1;
}

/* A file open exclusively (see the top of the file): over the old memo when the text fits its room, else in a free
 * run, else after the last memo; an empty value gives the old memo's room back. */
static int write_exclusive(xb_memo *m, uint32_t old, const char *p, size_t n, uint32_t *block, int32_t *err) {
  uint64_t need = n ? blocks_for(n) : 0;
  uint32_t room = 0;
  int had = old && old < m->next;
  if (had && !room_of(m, old, &room, err)) return 0;
  if (n && had && need <= room) {
    if (!put(m, old, p, n, err)) return 0;
    *block = old;
    if (need < room) give_back(m, old + (uint32_t)need, room - (uint32_t)need); /* the blocks it no longer takes */
    return 1;
  }
  if (n) { /* written first: the old memo's blocks go back only once the record can point at the new one */
    uint32_t at = need <= UINT32_MAX ? take(m, (uint32_t)need) : 0;
    if (at && !put(m, at, p, n, err)) {
      give_back(m, at, (uint32_t)need);
      return 0;
    }
    if (at) *block = at;
    else if (!append(m, p, n, block, err)) return 0;
  }
  if (had) give_back(m, old, room);
  return 1;
}

static int dbt_write(xb_memo *m, uint32_t old, const char *p, size_t n, uint32_t *block, int32_t *err) {
  *block = 0;
  if (xb_shared_exclusive(m->s)) return write_exclusive(m, old, p, n, block, err);
  if (!n) return 1;
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
  forget_runs(m);
  m->next = 1;
  return xb_platform_file_truncate(m->f, DBT_BLOCK, err) && write_next(m, err);
}

static int dbt_pack_begin(xb_memo *m, uint32_t *from, int32_t *err) {
  (void)err;
  forget_runs(m); /* the copies go after the last memo; the old blocks are cut */
  *from = m->next;
  return 1;
}

static int dbt_pack_copy(xb_memo *m, uint32_t block, uint32_t *copy, int32_t *err) {
  char *text;
  size_t len;
  *copy = 0;
  if (!read_text(m, block, &text, &len, err)) return 0;
  int ok = !len || append(m, text, len, copy, err);
  free(text);
  return ok;
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

static int dbt_sync(xb_memo *m, int32_t *err) { return xb_platform_file_sync(m->f, err); }

const xb_memo_format xb_memo_dbt = {
    .ext = ".dbt",
    .version = 0x83,
    .create = dbt_create,
    .open = dbt_open,
    .close = dbt_close,
    .sync = dbt_sync,
    .read = dbt_read,
    .write = dbt_write,
    .zap = dbt_zap,
    .pack_begin = dbt_pack_begin,
    .pack_copy = dbt_pack_copy,
    .pack_move = dbt_pack_move,
    .pack_end = dbt_pack_end,
};
