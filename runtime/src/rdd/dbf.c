/* The DBF table (dBASE III, as Clipper writes it), with its memos in a DBT file (rdd/dbt.c). Every binary field is
 * little-endian (core/bytes.h):
 *
 *   header, 32 bytes: 0 version (0x03, 0x83 with memo fields), 1-3 the date of the last update (year - 1900, month,
 *     day), 4-7 the record count, 8-9 the header's length, 10-11 the record's length, 29 the codepage byte, the rest
 *     zero;
 *   a 32-byte descriptor per field: 0-10 the name (NUL-padded), 11 the type, 16 the length, 17 the decimals (a
 *     character field's length takes both bytes, the decimals byte its high byte), the rest zero;
 *   0x0D and 0x00 after the descriptors (the header's length counts both);
 *   the records, each a deletion flag (' ' or '*') and the fields' text, back to back (a memo field: the number of
 *     the memo's first block, right-aligned in 10 characters; blanks for an empty memo);
 *   0x1A after the last record. */
#include "rdd/rdd.h"

#include "rdd/index.h"
#include "rdd/memo.h"

#include "core/bytes.h"
#include "core/file.h"
#include "core/shared_file.h"

#include <stdlib.h>
#include <string.h>

#define DBF_HEADER 32
#define DBF_FIELD 32
#define DBF_VERSION 0x03         /* a table without memo fields; with them, the memo format's (rdd/memo.h) */
#define DBF_FIELDS_END 0x0D
#define DBF_EOF 0x1A
#define DBF_DELETED '*'
#define DBF_MAX_FIELDS 2046      /* descriptors that fit the 16-bit header length */
#define DBF_MAX_NUMBER 255       /* the length byte of a number */
#define DBF_DATE_LEN 8           /* YYYYMMDD */
#define DBF_MEMO_LEN 10          /* a memo's block number, in digits */
#define DBF_MEMO_BINARY 4        /* the same as a 32-bit little-endian number (FPT tables) */
#define DBF_AHEAD 65536          /* bytes of records read at once when records are read in order */
#define DBF_FLAGS 28             /* the header's flags byte */
#define DBF_HAS_PRODUCTION 0x01  /* the table has a production order file (FoxPro's) */

typedef struct dbf {
  xb_shared *s;
  xb_file f;               /* s's handle */
  int shared;              /* opened shared: the count and records are read again, the header written on append */
  int readonly;            /* shared READONLY: it writes nothing, its locks are had without the OS */
  uint32_t count;          /* records */
  uint32_t headerLen, recordLen;
  unsigned char header[DBF_HEADER];
  unsigned char *record;   /* the current record, and the end mark after it (written with the last record) */
  uint32_t recno;          /* its number (count + 1: the blank record after the last) */
  int loaded;              /* the record's bytes are read (navigation reads them when they are first used) */
  int dirty;               /* the record changed since it was read */
  int changed;             /* the table changed: the header gets the date and count */
  int hasMemo;             /* the table has memo fields, and `memo` is open */
  const xb_memo_format *memoFormat;
  xb_memo *memo;
  unsigned char *ahead;    /* records read ahead: aheadCount of them from aheadFirst */
  uint32_t aheadFirst, aheadCount, lastRead;
  unsigned char *behind;   /* exclusive: the last records, written at the end and not to the file yet (behindCount
                              of them from behindFirst), then written together (drain) */
  uint32_t behindFirst, behindCount;
} dbf;

static dbf *state(xb_area *a) { return a->state; }

/* The record buffer as a blank record: blanks, a memo block number in 4 bytes as 0. */
static void blank(const xb_area *a, dbf *d) {
  memset(d->record, ' ', d->recordLen);
  for (uint32_t k = 0; k < a->nfields; ++k)
    if (a->fields[k].type == 'M' && a->fields[k].len == DBF_MEMO_BINARY)
      memset(d->record + a->fields[k].offset, 0, DBF_MEMO_BINARY);
}

static int64_t record_pos(const dbf *d, uint32_t recno) {
  return (int64_t)d->headerLen + (int64_t)(recno - 1) * d->recordLen;
}

static void put_today(unsigned char *p) {
  int y, m, d;
  xb_platform_local_date(&y, &m, &d);
  p[0] = (unsigned char)(y - 1900);
  p[1] = (unsigned char)m;
  p[2] = (unsigned char)d;
}

/* The lengths a field of type `type` can have here; the date and logical ones are fixed. */
static int field_fits(char type, uint32_t len, uint32_t dec) {
  switch (type) {
  case 'C': return len >= 1 && len <= UINT16_MAX && dec == 0;
  case 'N': case 'F': return len >= 1 && len <= DBF_MAX_NUMBER && (dec == 0 || dec + 1 < len);
  case 'D': return len == DBF_DATE_LEN && dec == 0;
  case 'L': return len == 1 && dec == 0;
  case 'M': return len == DBF_MEMO_LEN && dec == 0;
  default: return 0; /* other types come with their drivers */
  }
}



/* Whether a field of another program's table can be read and written: a known type, at least a byte, a whole date
 * and memo block number. */
static int field_readable(char type, uint32_t len) {
  return strchr("CNFDLM", type) && type && len >= 1 && (type != 'D' || len == DBF_DATE_LEN) &&
         (type != 'M' || len == DBF_MEMO_LEN || len == DBF_MEMO_BINARY);
}

/* The memo file of the table at `path` (malloc'ed): the format's extension (".dbt") as created; with `tableCase`, in
 * upper case (".DBT") when the table's extension is, the other name opening looks for where file names keep their
 * case. */
static char *memo_path(const char *path, const xb_memo_format *format, int tableCase) {
  size_t start, ext, len = strlen(path);
  xb_file_name_parts(path, len, &start, &ext);
  int upper = tableCase && ext + 1 < len;
  for (size_t i = ext + 1; upper && i < len; ++i) upper = path[i] < 'a' || path[i] > 'z';
  char *memo = xb_file_path_replace_ext(path, format->ext);
  for (size_t i = ext + 1; upper && memo[i]; ++i) memo[i] = xb_ascii_upper(memo[i]);
  return memo;
}

/* Opens the memo file of the table at `path`: ".dbt", else (not found) ".DBT" beside a table named in upper case.
 * The name tried first in `*tried` (malloc'ed) when it fails. */
static int open_memo(dbf *d, const char *path, int readonly, int32_t *err, char **tried) {
  const xb_memo_format *mf = d->memoFormat;
  *tried = memo_path(path, mf, 0);
  if (mf->open(*tried, readonly, !d->shared, &d->memo, err)) return 1;
  char *other = memo_path(path, mf, 1);
  int32_t otherErr = 0;
  int ok = *err == 2 && strcmp(other, *tried) != 0 && mf->open(other, readonly, !d->shared, &d->memo, &otherErr);
  free(other);
  return ok;
}

/* The format of a table's memos by its version byte: a table of plain fields has its driver's; NULL for one of
 * another format (dBASE IV, ...). */
static const xb_memo_format *memo_format_of(const xb_rdd *rdd, unsigned char version) {
  if (version == xb_memo_dbt.version) return &xb_memo_dbt;
  if (version == xb_memo_fpt.version) return &xb_memo_fpt;
  return version == DBF_VERSION ? rdd->memo : NULL;
}

static int has_memo(const xb_field_info *fields, uint32_t count) {
  for (uint32_t k = 0; k < count; ++k)
    if (fields[k].type == 'M') return 1;
  return 0;
}

int xb_dbf_check(const xb_rdd *rdd, xb_field_info *fields, uint32_t count) {
  uint32_t recordLen = 1;
  if (count > DBF_MAX_FIELDS) return 0;
  for (uint32_t k = 0; k < count; ++k) {
    xb_field_info *fi = &fields[k];
    if (fi->type == 'C' || fi->type == 'D' || fi->type == 'L' || fi->type == 'M') fi->dec = 0;
    if (fi->type == 'D') fi->len = DBF_DATE_LEN;
    if (fi->type == 'L') fi->len = 1;
    /* An FPT table's memo field holds the block number in 4 bytes when that is the width asked for. */
    int binary = fi->type == 'M' && rdd->memo == &xb_memo_fpt && fi->len == DBF_MEMO_BINARY;
    if (fi->type == 'M' && !binary) fi->len = DBF_MEMO_LEN;
    if (!binary && !field_fits(fi->type, fi->len, fi->dec)) return 0;
    recordLen += fi->len;
    if (recordLen > UINT16_MAX) return 0;
  }
  return 1;
}

static int dbf_create(const xb_rdd *rdd, const char *path, const xb_field_info *fields, uint32_t count,
                      int32_t *err) {
  size_t headerLen = DBF_HEADER + (size_t)count * DBF_FIELD + 2;
  unsigned char *h = calloc(headerLen + 1, 1);
  if (!h) xb_gc_fatal("out of memory");
  uint32_t recordLen = 1;
  for (uint32_t k = 0; k < count; ++k) {
    unsigned char *d = h + DBF_HEADER + (size_t)k * DBF_FIELD;
    memcpy(d, fields[k].name, strlen(fields[k].name));
    d[11] = (unsigned char)fields[k].type;
    if (fields[k].type == 'C') {
      xb_put_le16(d + 16, (uint16_t)fields[k].len);
    } else {
      d[16] = (unsigned char)fields[k].len;
      d[17] = (unsigned char)fields[k].dec;
    }
    recordLen += fields[k].len;
  }
  int memo = has_memo(fields, count);
  h[0] = memo ? rdd->memo->version : DBF_VERSION;
  put_today(h + 1);
  xb_put_le16(h + 8, (uint16_t)headerLen);
  xb_put_le16(h + 10, (uint16_t)recordLen);
  h[headerLen - 2] = DBF_FIELDS_END;
  h[headerLen] = DBF_EOF;
  xb_shared *s;
  int32_t closeErr = 0;
  int ok = xb_shared_open(path, 0, 1, 1, &s, err);
  if (ok) {
    ok = xb_platform_file_write_at(xb_shared_handle(s), 0, h, headerLen + 1, err);
    if (!xb_shared_close(s, &closeErr) && ok) {
      *err = closeErr;
      ok = 0;
    }
  }
  free(h);
  if (ok && memo) {
    char *memoPath = memo_path(path, rdd->memo, 0);
    ok = rdd->memo->create(memoPath, err);
    free(memoPath);
  }
  return ok;
}

static int dbf_open(xb_area *a, const char *path, int readonly, int exclusive, xb_open_failure *why) {
  int32_t *err = &why->oscode;
  dbf *d = calloc(1, sizeof *d);
  if (!d) xb_gc_fatal("out of memory");
  if (!xb_shared_open(path, readonly, exclusive, 0, &d->s, err)) {
    free(d);
    return 0;
  }
  d->f = xb_shared_handle(d->s);
  d->shared = !exclusive;
  d->readonly = readonly;
  unsigned char *h = NULL;
  xb_field_info *fields = NULL;
  uint32_t n = 0;
  if (!xb_file_read_at(d->f, 0, d->header, DBF_HEADER, err)) goto fail;
  d->count = xb_get_le32(d->header + 4);
  d->headerLen = xb_get_le16(d->header + 8);
  d->recordLen = xb_get_le16(d->header + 10);
  if (d->headerLen < DBF_HEADER + 1 || d->recordLen < 1) goto corrupt;
  h = malloc(d->headerLen);
  fields = malloc(((d->headerLen - DBF_HEADER) / DBF_FIELD + 1) * sizeof *fields);
  if (!h || !fields) xb_gc_fatal("out of memory");
  if (!xb_file_read_at(d->f, 0, h, d->headerLen, err)) goto fail;
  uint32_t offset = 1;
  for (size_t at = DBF_HEADER; at + DBF_FIELD <= d->headerLen && h[at] != DBF_FIELDS_END; at += DBF_FIELD) {
    const unsigned char *fd = h + at;
    xb_field_info *fi = &fields[n++];
    memset(fi->name, 0, sizeof fi->name);
    for (size_t i = 0; i < XB_FIELD_NAME_SIZE - 1 && fd[i]; ++i) fi->name[i] = xb_ascii_upper((char)fd[i]);
    fi->type = (char)fd[11];
    fi->len = fi->type == 'C' ? xb_get_le16(fd + 16) : fd[16];
    fi->dec = fi->type == 'C' ? 0 : fd[17];
    fi->offset = offset;
    offset += fi->len;
    if (!field_readable(fi->type, fi->len)) goto corrupt;
  }
  if (offset > d->recordLen) goto corrupt;
  d->memoFormat = memo_format_of(a->rdd, d->header[0]);
  if (has_memo(fields, n) && !d->memoFormat) {
    why->gencode = EG_CORRUPTION; /* memos of another format (dBASE IV, ...) */
    why->subcode = 1012;
    goto fail;
  }
  if (has_memo(fields, n)) {
    char *memoPath;
    if (!open_memo(d, path, readonly, err, &memoPath)) {
      why->subcode = 1002;
      why->file = memoPath;
      goto fail;
    }
    free(memoPath);
    d->hasMemo = 1;
  }
  d->record = malloc(d->recordLen + 1u);
  if (!d->record) xb_gc_fatal("out of memory");
  d->record[d->recordLen] = DBF_EOF;
  a->fields = fields;
  a->nfields = n;
  blank(a, d);
  d->recno = d->count + 1;
  d->loaded = 1;
  free(h);
  a->state = d;
  return 1;
corrupt:
  *err = XB_FERR_FAILURE;
fail:
  free(h);
  free(fields);
  int32_t ignored;
  xb_shared_close(d->s, &ignored);
  free(d);
  return 0;
}

static int write_header(dbf *d, int32_t *err) {
  put_today(d->header + 1);
  xb_put_le32(d->header + 4, d->count);
  if (!xb_platform_file_write_at(d->f, 0, d->header, DBF_HEADER, err)) return 0;
  d->changed = 0;
  return 1;
}

/* Writes the records kept behind, the end mark after the last one. */
static int drain(dbf *d, int32_t *err) {
  if (!d->behindCount) return 1;
  size_t n = (size_t)d->behindCount * d->recordLen;
  if (d->behindFirst + d->behindCount - 1 == d->count) d->behind[n++] = DBF_EOF;
  d->behindCount = 0;
  return xb_platform_file_write_at(d->f, record_pos(d, d->behindFirst), d->behind, n, err);
}

/* Writes the current record. Exclusive, a record at the end is kept behind with the ones before it, which go to the
 * file together: appends cost one write a chunk instead of one a record. */
static int write_record(dbf *d, int32_t *err) {
  uint32_t r = d->recno;
  size_t len = d->recordLen;
  if (!d->shared && d->behindCount && r - d->behindFirst < d->behindCount) {
    memcpy(d->behind + (size_t)(r - d->behindFirst) * len, d->record, len);
    return 1;
  }
  if (!d->shared && r == d->count && len + 1 <= DBF_AHEAD) {
    if (d->behindCount && (r != d->behindFirst + d->behindCount || (d->behindCount + 1) * len + 1 > DBF_AHEAD) &&
        !drain(d, err))
      return 0;
    if (!d->behind && !(d->behind = malloc(DBF_AHEAD))) xb_gc_fatal("out of memory");
    if (!d->behindCount) d->behindFirst = r;
    memcpy(d->behind + (size_t)d->behindCount++ * len, d->record, len);
    return 1;
  }
  if (!drain(d, err)) return 0;
  /* The last record: the end mark after it; shared, another program's record may follow it (append wrote it). */
  return xb_platform_file_write_at(d->f, record_pos(d, r), d->record, len + (r == d->count && !d->shared ? 1u : 0u),
                                   err);
}

static int dbf_flush(xb_area *a, int header, int32_t *err) {
  dbf *d = state(a);
  if (d->dirty) {
    if (!write_record(d, err)) return 0;
    if (d->recno - d->aheadFirst < d->aheadCount) /* the copy read ahead too */
      memcpy(d->ahead + (size_t)(d->recno - d->aheadFirst) * d->recordLen, d->record, d->recordLen);
    d->dirty = 0;
  }
  if (header && !drain(d, err)) return 0;
  return !header || !d->changed || d->shared || write_header(d, err); /* shared: only append writes it */
}

static int dbf_close(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  int ok = dbf_flush(a, 1, err);
  int32_t closeErr = 0;
  if (!xb_shared_close(d->s, &closeErr) && ok) {
    *err = closeErr;
    ok = 0;
  }
  if (d->hasMemo && !d->memoFormat->close(d->memo, &closeErr) && ok) {
    *err = closeErr;
    ok = 0;
  }
  free(d->record);
  free(d->ahead);
  free(d->behind);
  free(d);
  free(a->fields);
  a->state = NULL;
  a->fields = NULL;
  return ok;
}

/* Shared: the record count from the file's size, which other programs' appends make grow before the header says
 * so. 0 when the size cannot be had (the count stays). */
static int count_again(dbf *d, int32_t *err) {
  int64_t size = xb_platform_file_seek(d->f, 0, FS_END, err);
  if (size < 0) return 0;
  if (size < (int64_t)d->headerLen) return 1;
  int64_t n = (size - d->headerLen) / d->recordLen;
  d->count = n > UINT32_MAX ? UINT32_MAX : (uint32_t)n;
  return 1;
}

static uint32_t dbf_lastrec(xb_area *a) {
  dbf *d = state(a);
  int32_t ignored;
  if (d->shared && !a->snapshot) count_again(d, &ignored);
  return d->count;
}

/* Reads record `recno` into the record buffer: from the records read ahead, or with the ones after it when the
 * records are read in order, when `ahead` allows (a shared table: under the file lock only). */
static int read_record(dbf *d, uint32_t recno, int ahead, int32_t *err) {
  if (d->behindCount && recno - d->behindFirst < d->behindCount) {
    memcpy(d->record, d->behind + (size_t)(recno - d->behindFirst) * d->recordLen, d->recordLen);
    return 1;
  }
  if (ahead && recno - d->aheadFirst < d->aheadCount) {
    memcpy(d->record, d->ahead + (size_t)(recno - d->aheadFirst) * d->recordLen, d->recordLen);
    return 1;
  }
  if (!drain(d, err)) return 0; /* the file has every record before it is read */
  uint32_t n = DBF_AHEAD / d->recordLen;
  if (recno != d->lastRead + 1 || n < 2 || !ahead)
    return xb_file_read_at(d->f, record_pos(d, recno), d->record, d->recordLen, err);
  if (n > d->count - recno + 1) n = d->count - recno + 1;
  if (!d->ahead && !(d->ahead = malloc(DBF_AHEAD))) xb_gc_fatal("out of memory");
  d->aheadCount = 0;
  if (!xb_file_read_at(d->f, record_pos(d, recno), d->ahead, (size_t)n * d->recordLen, err)) return 0;
  d->aheadFirst = recno;
  d->aheadCount = n;
  memcpy(d->record, d->ahead, d->recordLen);
  return 1;
}

/* Makes record `recno` current; its bytes are read when first used (dbf_load). */
static int dbf_go(xb_area *a, uint32_t recno, int32_t *err) {
  XB_UNUSED(err);
  dbf *d = state(a);
  if (recno >= 1 && recno <= d->count) {
    if (recno != d->recno || d->shared) d->loaded = 0;
    d->recno = recno;
    return 1;
  }
  d->recno = d->count + 1; /* the blank record after the last */
  blank(a, d);
  d->loaded = 1;
  return 1;
}

static int dbf_load(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  if (d->loaded) return 1;
  if (!read_record(d, d->recno, !d->shared || a->flocked || a->snapshot, err)) { /* else the blank record */
    d->recno = d->count + 1;
    blank(a, d);
    d->loaded = 1;
    return 0;
  }
  d->lastRead = d->recno;
  d->loaded = 1;
  return 1;
}

/* The bytes of record `recno`'s lock (0: the file lock's) and of the append lock, by the driver's scheme. */
static void lock_range(const xb_area *a, const dbf *d, uint32_t recno, int high, uint64_t *pos, uint64_t *len) {
  if (a->rdd->lockScheme == XB_LOCKS_FOXPRO && high) {
    *pos = (uint64_t)XB_FOX_HIGH_LOCK - (recno ? recno : XB_FOX_HIGH_FILE);
    *len = recno ? 1 : XB_FOX_HIGH_FILE;
    return;
  }
  if (a->rdd->lockScheme == XB_LOCKS_FOXPRO) {
    *pos = recno ? XB_FOX_LOCK + (uint64_t)(recno - 1) * d->recordLen + d->headerLen : XB_FOX_LOCK + 1u;
    *len = recno ? 1 : XB_FOX_LOCK - 3u;
    return;
  }
  *pos = (uint64_t)XB_DB_LOCK + (recno ? recno : 1u);
  *len = recno ? 1 : XB_DB_LOCK;
}

static uint64_t append_lock(const xb_area *a) {
  if (a->rdd->lockScheme != XB_LOCKS_FOXPRO) return XB_DB_LOCK;
  return xb_area_production_open(a) ? XB_FOX_HIGH_LOCK : XB_FOX_LOCK;
}

/* Shared: under the append lock, the record written at once after the last one in the file now, locked (unless the
 * file is), and the header with the new count. */
static int append_shared(xb_area *a, dbf *d, int32_t *err) {
  if (!xb_shared_lock(d->s, append_lock(a), 1, XB_LOCK_WRITE, 1, err)) return 0;
  int ok = count_again(d, err);
  uint32_t recno = d->count + 1;
  int locked = !a->flocked;
  if (ok && recno == 0) { /* no record number left */
    *err = XB_FERR_FAILURE;
    ok = 0;
  }
  uint64_t pos, len;
  lock_range(a, d, recno, xb_area_production_open(a), &pos, &len);
  ok = ok && (!locked || xb_shared_lock(d->s, pos, len, XB_LOCK_WRITE, 0, err));
  if (ok) {
    blank(a, d);
    d->record[d->recordLen] = DBF_EOF;
    ok = xb_platform_file_write_at(d->f, record_pos(d, recno), d->record, d->recordLen + 1u, err);
    if (ok) {
      d->count = recno;
      ok = write_header(d, err);
    }
    int32_t ignored;
    if (!ok && locked) xb_shared_unlock(d->s, pos, len, &ignored);
  }
  int32_t unlockErr = 0;
  if (!xb_shared_unlock(d->s, append_lock(a), 1, &unlockErr) && ok) {
    *err = unlockErr;
    ok = 0;
  }
  if (ok) d->recno = recno;
  return ok;
}

static int dbf_append(xb_area *a, uint32_t *recno, int32_t *err) {
  dbf *d = state(a);
  if (!dbf_flush(a, 0, err)) return 0;
  if (d->shared) {
    if (!append_shared(a, d, err)) return 0;
    d->loaded = 1;
  } else {
    d->recno = ++d->count;
    blank(a, d);
    d->loaded = d->dirty = d->changed = 1;
  }
  *recno = d->recno;
  return 1;
}

static void dbf_refresh(xb_area *a) {
  dbf *d = state(a);
  if (d->shared && !d->dirty && d->recno <= d->count) d->loaded = 0;
}

/* Record `recno`'s lock (0: the file's), taken (`on`) or released; held by nobody else: 1, else 0 with 33. A lock
 * had on the current record (or the file) makes it read again: what was read before may be another program's past.
 * Records read ahead are dropped when the file lock comes or goes. */
static int dbf_lock(xb_area *a, uint32_t recno, int on, int32_t *err) {
  dbf *d = state(a);
  if (!d->shared || d->readonly) return 1;
  uint64_t pos, len;
  int high = xb_area_production_open(a);
  lock_range(a, d, recno, high, &pos, &len);
  if (!recno) d->aheadCount = 0;
  if (!on) { /* FoxPro's: the production file may have opened or closed since the lock was taken; a range not held is
                nothing to release */
    int ok = xb_shared_unlock(d->s, pos, len, err);
    if (a->rdd->lockScheme != XB_LOCKS_FOXPRO) return ok;
    lock_range(a, d, recno, !high, &pos, &len);
    return xb_shared_unlock(d->s, pos, len, err) && ok;
  }
  int ok = xb_shared_lock(d->s, pos, len, XB_LOCK_WRITE, 0, err);
  if (ok && (!recno || recno == d->recno) && d->recno <= d->count) d->loaded = 0;
  return ok;
}

/* The block number a memo field of `len` bytes holds (digits after blanks; 4 bytes: a 32-bit little-endian number),
 * 0 for none. */
static uint32_t memo_block(const char *p, uint32_t len) {
  if (len == DBF_MEMO_BINARY) return xb_get_le32((const unsigned char *)p);
  uint64_t n = 0;
  size_t i = 0;
  while (i < DBF_MEMO_LEN && p[i] == ' ') ++i;
  for (; i < DBF_MEMO_LEN && p[i] >= '0' && p[i] <= '9' && n <= UINT32_MAX; ++i) n = n * 10 + (uint64_t)(p[i] - '0');
  return n <= UINT32_MAX ? (uint32_t)n : 0;
}

/* Writes `block` as a memo field's text: right-aligned digits, blanks for 0 (4 bytes: the number). */
static void put_memo_block(char *p, uint32_t len, uint32_t block) {
  if (len == DBF_MEMO_BINARY) {
    xb_put_le32((unsigned char *)p, block);
    return;
  }
  memset(p, ' ', DBF_MEMO_LEN);
  for (int i = DBF_MEMO_LEN - 1; block && i >= 0; --i, block /= 10) p[i] = (char)('0' + block % 10);
}

static int dbf_get(xb_area *a, uint32_t field, xb_item *r, int32_t *err) {
  dbf *d = state(a);
  if (!dbf_load(a, err)) return 0;
  const xb_field_info *fi = &a->fields[field];
  const char *p = (const char *)d->record + fi->offset;
  if (fi->type != 'M') {
    xb_field_decode(fi, p, r);
    return 1;
  }
  if (!d->memoFormat->read(d->memo, memo_block(p, fi->len), r, err)) return 0;
  if (r->type == XB_T_STRING) r->width = XB_STR_MEMO;
  return 1;
}

static int dbf_put(xb_area *a, uint32_t field, const xb_item *v, int32_t *err) {
  dbf *d = state(a);
  if (!dbf_load(a, err)) return XB_PUT_FAILED;
  const xb_field_info *fi = &a->fields[field];
  char *p = (char *)d->record + fi->offset;
  int r = XB_PUT_OK;
  if (fi->type != 'M') {
    r = xb_field_encode(fi, p, v);
    if (r == XB_PUT_TYPE) return r;
  } else { /* the memo file is written at once; an empty memo is none */
    if (v->type != XB_T_STRING) return XB_PUT_TYPE;
    uint32_t block = 0;
    if (!d->memoFormat->write(d->memo, memo_block(p, fi->len), v->v.s->data, v->v.s->len, &block, err))
      return XB_PUT_FAILED;
    put_memo_block(p, fi->len, block);
  }
  d->dirty = d->changed = 1;
  return r;
}

static const unsigned char *dbf_field_bytes(xb_area *a, uint32_t field) {
  return state(a)->record + a->fields[field].offset;
}

static const unsigned char *dbf_record_bytes(xb_area *a) { return state(a)->record + 1; }

static int dbf_put_record_bytes(xb_area *a, const unsigned char *bytes, int32_t *err) {
  dbf *d = state(a);
  if (!dbf_load(a, err)) return 0;
  memcpy(d->record + 1, bytes, d->recordLen - 1);
  d->dirty = d->changed = 1;
  return 1;
}

static int dbf_deleted(xb_area *a) { return state(a)->record[0] == DBF_DELETED; }

static void dbf_set_deleted(xb_area *a, int deleted) {
  dbf *d = state(a);
  d->record[0] = deleted ? DBF_DELETED : ' ';
  d->dirty = d->changed = 1;
}

/* Cuts the file after `count` records and puts the end mark there. */
static int cut(dbf *d, int32_t *err) {
  int64_t end = record_pos(d, d->count + 1);
  unsigned char eof = DBF_EOF;
  d->changed = 1;
  return xb_platform_file_truncate(d->f, end, err) && xb_platform_file_write_at(d->f, end, &eof, 1, err);
}

/* Copies the memos of the current record after the last memo: the block of each copy in `blocks`, one per memo
 * field (0: none). */
static int copy_memos(xb_area *a, uint32_t *blocks, int32_t *err) {
  dbf *d = state(a);
  for (uint32_t k = 0; k < a->nfields; ++k) {
    if (a->fields[k].type != 'M') continue;
    uint32_t block = memo_block((const char *)d->record + a->fields[k].offset, a->fields[k].len);
    *blocks = 0;
    if (block && !d->memoFormat->pack_copy(d->memo, block, blocks, err)) return 0;
    ++blocks;
  }
  return 1;
}

/* The memo fields of the current record: the blocks in `blocks`, less `shift` (none: 0). */
static void set_memos(xb_area *a, const uint32_t *blocks, uint32_t shift) {
  dbf *d = state(a);
  for (uint32_t k = 0; k < a->nfields; ++k) {
    if (a->fields[k].type != 'M') continue;
    char *p = (char *)d->record + a->fields[k].offset;
    uint32_t block = blocks ? *blocks++ : memo_block(p, a->fields[k].len);
    put_memo_block(p, a->fields[k].len, block ? block - shift : 0);
  }
}

/* PACK with memos goes in steps that each leave every record on a whole memo, so that a failure (a full disk) at any
 * of them loses nothing (rdd/memo.h): the memos of the records kept are copied after every older memo; the records
 * kept move to the front with their copies' blocks; the copies move to the first block when they fit before
 * themselves, the records follow, and the older memos are cut. */
static int pack(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  uint32_t memos = 0, kept = 0, from = 0, shift = 0;
  for (uint32_t k = 0; k < a->nfields; ++k) memos += a->fields[k].type == 'M';
  if (memos && d->count > SIZE_MAX / sizeof(uint32_t) / memos) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  uint32_t *blocks = memos ? malloc(((size_t)d->count * memos + 1) * sizeof *blocks) : NULL;
  if (memos && !blocks) xb_gc_fatal("out of memory");
  int ok = !memos || d->memoFormat->pack_begin(d->memo, &from, err);
  for (uint32_t k = 1; ok && memos && k <= d->count; ++k) {
    ok = xb_file_read_at(d->f, record_pos(d, k), d->record, d->recordLen, err);
    if (ok && d->record[0] != DBF_DELETED) ok = copy_memos(a, blocks + (size_t)kept++ * memos, err);
  }
  kept = 0;
  for (uint32_t k = 1; ok && k <= d->count; ++k) {
    ok = xb_file_read_at(d->f, record_pos(d, k), d->record, d->recordLen, err);
    if (!ok || d->record[0] == DBF_DELETED) continue;
    if (memos) set_memos(a, blocks + (size_t)kept * memos, 0);
    ++kept;
    if (kept != k || memos) ok = xb_platform_file_write_at(d->f, record_pos(d, kept), d->record, d->recordLen, err);
  }
  free(blocks);
  if (!ok) return 0;
  d->count = kept;
  if (!cut(d, err)) return 0;
  if (memos && !d->memoFormat->pack_move(d->memo, from, &shift, err)) return 0;
  for (uint32_t k = 1; shift && k <= d->count; ++k) {
    if (!xb_file_read_at(d->f, record_pos(d, k), d->record, d->recordLen, err)) return 0;
    set_memos(a, NULL, shift);
    if (!xb_platform_file_write_at(d->f, record_pos(d, k), d->record, d->recordLen, err)) return 0;
  }
  return !memos || d->memoFormat->pack_end(d->memo, from, shift, err);
}

static int dbf_pack(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  d->aheadCount = 0;
  int ok = dbf_flush(a, 0, err) && drain(d, err) && pack(a, err);
  d->recno = d->count + 1; /* the record read last is no longer the current one */
  blank(a, d);
  d->loaded = 1;
  return ok;
}

static int dbf_zap(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  d->aheadCount = 0;
  d->behindCount = 0; /* records ZAP removes */
  d->dirty = 0;
  d->count = 0;
  d->recno = 1; /* the blank record after the last */
  blank(a, d);
  d->loaded = 1;
  return cut(d, err) && (!d->hasMemo || d->memoFormat->zap(d->memo, err));
}

static uint32_t dbf_header_size(xb_area *a) { return state(a)->headerLen; }
static uint32_t dbf_record_size(xb_area *a) { return state(a)->recordLen; }

static int64_t dbf_last_update(xb_area *a) {
  const unsigned char *h = state(a)->header;
  return xb_date_encode(1900 + h[1], h[2], h[3]);
}

/* The flag is written with the header (its date and count too) in an exclusive table, alone in a shared one. */
static int dbf_set_production(xb_area *a, int on, int32_t *err) {
  dbf *d = state(a);
  unsigned char flags = (unsigned char)(on ? d->header[DBF_FLAGS] | DBF_HAS_PRODUCTION
                                           : d->header[DBF_FLAGS] & ~DBF_HAS_PRODUCTION);
  if (flags == d->header[DBF_FLAGS] || d->readonly || a->readonly) return 1;
  d->header[DBF_FLAGS] = flags;
  if (!d->shared) return write_header(d, err);
  if (!xb_shared_lock(d->s, append_lock(a), 1, XB_LOCK_WRITE, 1, err)) return 0; /* the header's lock */
  int ok = xb_platform_file_write_at(d->f, DBF_FLAGS, &flags, 1, err);
  int32_t unlockErr = 0;
  if (!xb_shared_unlock(d->s, append_lock(a), 1, &unlockErr) && ok) {
    *err = unlockErr;
    ok = 0;
  }
  return ok;
}

const xb_rdd xb_rdd_dbf = {
    .name = "DBFNTX",
    .ext = ".dbf",
    .memo = &xb_memo_dbt,
    .index = &xb_index_ntx,
    .lockScheme = XB_LOCKS_CLIPPER,
    .check = xb_dbf_check,
    .create = dbf_create,
    .open = dbf_open,
    .close = dbf_close,
    .lastrec = dbf_lastrec,
    .go = dbf_go,
    .flush = dbf_flush,
    .append = dbf_append,
    .lock = dbf_lock,
    .get = dbf_get,
    .put = dbf_put,
    .field_bytes = dbf_field_bytes,
    .record_bytes = dbf_record_bytes,
    .put_record_bytes = dbf_put_record_bytes,
    .load = dbf_load,
    .refresh = dbf_refresh,
    .deleted = dbf_deleted,
    .set_deleted = dbf_set_deleted,
    .pack = dbf_pack,
    .zap = dbf_zap,
    .header_size = dbf_header_size,
    .record_size = dbf_record_size,
    .last_update = dbf_last_update,
    .set_production = dbf_set_production,
};

const xb_rdd xb_rdd_dbfcdx = {
    .name = "DBFCDX",
    .ext = ".dbf",
    .memo = &xb_memo_fpt,
    .index = &xb_index_cdx,
    .lockScheme = XB_LOCKS_FOXPRO,
    .check = xb_dbf_check,
    .create = dbf_create,
    .open = dbf_open,
    .close = dbf_close,
    .lastrec = dbf_lastrec,
    .go = dbf_go,
    .flush = dbf_flush,
    .append = dbf_append,
    .lock = dbf_lock,
    .get = dbf_get,
    .put = dbf_put,
    .field_bytes = dbf_field_bytes,
    .record_bytes = dbf_record_bytes,
    .put_record_bytes = dbf_put_record_bytes,
    .load = dbf_load,
    .refresh = dbf_refresh,
    .deleted = dbf_deleted,
    .set_deleted = dbf_set_deleted,
    .pack = dbf_pack,
    .zap = dbf_zap,
    .header_size = dbf_header_size,
    .record_size = dbf_record_size,
    .last_update = dbf_last_update,
    .set_production = dbf_set_production,
};
