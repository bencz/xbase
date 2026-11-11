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

#include "rdd/dbt.h"

#include "core/bytes.h"
#include "core/file.h"
#include "core/numeric.h"

#include <stdlib.h>
#include <string.h>

#define DBF_HEADER 32
#define DBF_FIELD 32
#define DBF_VERSION 0x03
#define DBF_VERSION_MEMO 0x83
#define DBF_FIELDS_END 0x0D
#define DBF_EOF 0x1A
#define DBF_DELETED '*'
#define DBF_MAX_FIELDS 2046      /* descriptors that fit the 16-bit header length */
#define DBF_MAX_NUMBER 255       /* the length byte of a number */
#define DBF_DATE_LEN 8           /* YYYYMMDD */
#define DBF_MEMO_LEN 10          /* a memo's block number */
#define DBF_AHEAD 65536          /* bytes of records read at once when records are read in order */

typedef struct dbf {
  xb_file f;
  uint32_t count;          /* records */
  uint32_t headerLen, recordLen;
  unsigned char header[DBF_HEADER];
  unsigned char *record;   /* the current record, and the end mark after it (written with the last record) */
  uint32_t recno;          /* its number (count + 1: the blank record after the last) */
  int loaded;              /* the record's bytes are read (navigation reads them when they are first used) */
  int dirty;               /* the record changed since it was read */
  int changed;             /* the table changed: the header gets the date and count */
  int hasMemo;             /* the table has memo fields, and `memo` is open */
  xb_dbt memo;
  unsigned char *ahead;    /* records read ahead: aheadCount of them from aheadFirst */
  uint32_t aheadFirst, aheadCount, lastRead;
} dbf;

static dbf *state(xb_area *a) { return a->state; }

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
         (type != 'M' || len == DBF_MEMO_LEN);
}

/* The memo file of the table at `path` (malloc'ed): ".dbt" as created; with `tableCase`, ".DBT" when the table's
 * extension is upper case ("DATA.DBF"), the other name opening looks for where file names keep their case. */
static char *memo_path(const char *path, int tableCase) {
  size_t start, ext, len = strlen(path);
  xb_file_name_parts(path, len, &start, &ext);
  int upper = tableCase && ext + 1 < len;
  for (size_t i = ext + 1; upper && i < len; ++i) upper = path[i] < 'a' || path[i] > 'z';
  char *memo = xb_file_path_replace_ext(path, XB_DBT_EXT);
  for (size_t i = ext + 1; upper && memo[i]; ++i) memo[i] = xb_ascii_upper(memo[i]);
  return memo;
}

/* Opens the memo file of the table at `path`: ".dbt", else (not found) ".DBT" beside a table named in upper case.
 * The name tried first in `*tried` (malloc'ed) when it fails. */
static int open_memo(dbf *d, const char *path, int readonly, int32_t *err, char **tried) {
  *tried = memo_path(path, 0);
  if (xb_dbt_open(&d->memo, *tried, readonly, err)) return 1;
  char *other = memo_path(path, 1);
  int32_t otherErr = 0;
  int ok = *err == 2 && strcmp(other, *tried) != 0 && xb_dbt_open(&d->memo, other, readonly, &otherErr);
  free(other);
  return ok;
}

static int has_memo(const xb_field_info *fields, uint32_t count) {
  for (uint32_t k = 0; k < count; ++k)
    if (fields[k].type == 'M') return 1;
  return 0;
}

static int dbf_check(xb_field_info *fields, uint32_t count) {
  uint32_t recordLen = 1;
  if (count > DBF_MAX_FIELDS) return 0;
  for (uint32_t k = 0; k < count; ++k) {
    xb_field_info *fi = &fields[k];
    if (fi->type == 'C' || fi->type == 'D' || fi->type == 'L' || fi->type == 'M') fi->dec = 0;
    if (fi->type == 'D') fi->len = DBF_DATE_LEN;
    if (fi->type == 'L') fi->len = 1;
    if (fi->type == 'M') fi->len = DBF_MEMO_LEN;
    if (!field_fits(fi->type, fi->len, fi->dec)) return 0;
    recordLen += fi->len;
    if (recordLen > UINT16_MAX) return 0;
  }
  return 1;
}

static int dbf_create(const char *path, const xb_field_info *fields, uint32_t count, int32_t *err) {
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
  h[0] = memo ? DBF_VERSION_MEMO : DBF_VERSION;
  put_today(h + 1);
  xb_put_le16(h + 8, (uint16_t)headerLen);
  xb_put_le16(h + 10, (uint16_t)recordLen);
  h[headerLen - 2] = DBF_FIELDS_END;
  h[headerLen] = DBF_EOF;
  xb_file f;
  int32_t closeErr = 0;
  int ok = xb_platform_file_create(path, FC_NORMAL, &f, err);
  if (ok) {
    ok = xb_platform_file_write(f, h, headerLen + 1, err);
    if (!xb_platform_file_close(f, &closeErr) && ok) {
      *err = closeErr;
      ok = 0;
    }
  }
  free(h);
  if (ok && memo) {
    char *memoPath = memo_path(path, 0);
    ok = xb_dbt_create(memoPath, err);
    free(memoPath);
  }
  return ok;
}

static int dbf_open(xb_area *a, const char *path, int readonly, xb_open_failure *why) {
  int32_t *err = &why->oscode;
  dbf *d = calloc(1, sizeof *d);
  if (!d) xb_gc_fatal("out of memory");
  if (!xb_platform_file_open(path, readonly ? FO_READ : FO_READWRITE, &d->f, err)) {
    free(d);
    return 0;
  }
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
  if (has_memo(fields, n) && d->header[0] != DBF_VERSION && d->header[0] != DBF_VERSION_MEMO) {
    why->gencode = EG_CORRUPTION; /* memos of another format (dBASE IV, FoxPro) */
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
  memset(d->record, ' ', d->recordLen);
  d->recno = d->count + 1;
  d->loaded = 1;
  free(h);
  a->state = d;
  a->fields = fields;
  a->nfields = n;
  return 1;
corrupt:
  *err = XB_FERR_FAILURE;
fail:
  free(h);
  free(fields);
  int32_t ignored;
  xb_platform_file_close(d->f, &ignored);
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

static int dbf_flush(xb_area *a, int header, int32_t *err) {
  dbf *d = state(a);
  if (d->dirty) {
    size_t n = d->recordLen + (d->recno == d->count ? 1u : 0u); /* the last record: the end mark after it */
    if (!xb_platform_file_write_at(d->f, record_pos(d, d->recno), d->record, n, err)) return 0;
    if (d->recno - d->aheadFirst < d->aheadCount) /* the copy read ahead too */
      memcpy(d->ahead + (size_t)(d->recno - d->aheadFirst) * d->recordLen, d->record, d->recordLen);
    d->dirty = 0;
  }
  return !header || !d->changed || write_header(d, err);
}

static int dbf_close(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  int ok = dbf_flush(a, 1, err);
  int32_t closeErr = 0;
  if (!xb_platform_file_close(d->f, &closeErr) && ok) {
    *err = closeErr;
    ok = 0;
  }
  if (d->hasMemo && !xb_dbt_close(&d->memo, &closeErr) && ok) {
    *err = closeErr;
    ok = 0;
  }
  free(d->record);
  free(d->ahead);
  free(d);
  free(a->fields);
  a->state = NULL;
  a->fields = NULL;
  return ok;
}

static uint32_t dbf_lastrec(xb_area *a) { return state(a)->count; }

/* Reads record `recno` into the record buffer: from the records read ahead, or with the ones after it when the
 * records are read in order. */
static int read_record(dbf *d, uint32_t recno, int32_t *err) {
  if (recno - d->aheadFirst < d->aheadCount) {
    memcpy(d->record, d->ahead + (size_t)(recno - d->aheadFirst) * d->recordLen, d->recordLen);
    return 1;
  }
  uint32_t n = DBF_AHEAD / d->recordLen;
  if (recno != d->lastRead + 1 || n < 2)
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
    if (recno != d->recno) d->loaded = 0;
    d->recno = recno;
    return 1;
  }
  d->recno = d->count + 1; /* the blank record after the last */
  memset(d->record, ' ', d->recordLen);
  d->loaded = 1;
  return 1;
}

static int dbf_load(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  if (d->loaded) return 1;
  if (!read_record(d, d->recno, err)) { /* the blank record after the last instead */
    d->recno = d->count + 1;
    memset(d->record, ' ', d->recordLen);
    d->loaded = 1;
    return 0;
  }
  d->lastRead = d->recno;
  d->loaded = 1;
  return 1;
}

static int dbf_append(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  if (!dbf_flush(a, 0, err)) return 0;
  d->recno = ++d->count;
  memset(d->record, ' ', d->recordLen);
  d->loaded = d->dirty = d->changed = 1;
  return 1;
}

/* The block number a memo field holds (digits after blanks), 0 for none. */
static uint32_t memo_block(const char *p) {
  uint64_t n = 0;
  size_t i = 0;
  while (i < DBF_MEMO_LEN && p[i] == ' ') ++i;
  for (; i < DBF_MEMO_LEN && p[i] >= '0' && p[i] <= '9' && n <= UINT32_MAX; ++i) n = n * 10 + (uint64_t)(p[i] - '0');
  return n <= UINT32_MAX ? (uint32_t)n : 0;
}

/* Writes `block` as a memo field's text: right-aligned digits, blanks for 0. */
static void put_memo_block(char *p, uint32_t block) {
  memset(p, ' ', DBF_MEMO_LEN);
  for (int i = DBF_MEMO_LEN - 1; block && i >= 0; --i, block /= 10) p[i] = (char)('0' + block % 10);
}

static int dbf_get(xb_area *a, uint32_t field, xb_item *r, int32_t *err) {
  dbf *d = state(a);
  if (!dbf_load(a, err)) return 0;
  const xb_field_info *fi = &a->fields[field];
  const char *p = (const char *)d->record + fi->offset;
  switch (fi->type) {
  case 'C':
    xb_set_str(r, xb_str_new(p, fi->len));
    return 1;
  case 'M':
    if (!xb_dbt_read(&d->memo, memo_block(p), r, err)) return 0;
    r->width = XB_STR_MEMO;
    return 1;
  case 'D': {
    int v[DBF_DATE_LEN], digits = 1;
    for (int i = 0; i < DBF_DATE_LEN; ++i) {
      v[i] = p[i] - '0';
      digits &= v[i] >= 0 && v[i] <= 9;
    }
    int64_t jd = digits ? xb_date_encode(v[0] * 1000 + v[1] * 100 + v[2] * 10 + v[3], v[4] * 10 + v[5],
                                         v[6] * 10 + v[7])
                        : 0;
    xb_set_date(r, jd);
    return 1;
  }
  case 'L':
    xb_set_logical(r, p[0] && strchr("TtYy", p[0]));
    return 1;
  default: { /* N, F: the text's number, with the field's width and decimals */
    xb_num_parse(p, fi->len, r);
    if (fi->dec == 0 && r->type == XB_T_INT) {
      xb_put(r, xb_header_word(XB_T_INT, (uint16_t)fi->len, 0), (uint64_t)r->v.i);
      return 1;
    }
    double v = r->type == XB_T_INT ? (double)r->v.i : r->v.d;
    uint64_t bits;
    memcpy(&bits, &v, 8);
    uint32_t width = fi->dec ? (fi->len > fi->dec + 1 ? fi->len - fi->dec - 1 : 1) : fi->len;
    xb_put(r, xb_header_word(XB_T_DOUBLE, (uint16_t)width, (uint16_t)fi->dec), bits);
    return 1;
  }
  }
}

static int dbf_put(xb_area *a, uint32_t field, const xb_item *v, int32_t *err) {
  dbf *d = state(a);
  if (!dbf_load(a, err)) return XB_PUT_FAILED;
  const xb_field_info *fi = &a->fields[field];
  char *p = (char *)d->record + fi->offset;
  int r = XB_PUT_OK;
  switch (fi->type) {
  case 'C': {
    if (v->type != XB_T_STRING) return XB_PUT_TYPE;
    size_t n = v->v.s->len < fi->len ? v->v.s->len : fi->len;
    memcpy(p, v->v.s->data, n);
    memset(p + n, ' ', fi->len - n);
    break;
  }
  case 'D': {
    if (v->type != XB_T_DATE) return XB_PUT_TYPE;
    char digits[XB_DATE_DIGITS_SIZE]; /* blanks for the empty date */
    xb_date_to_digits(v->v.i, digits);
    memcpy(p, digits, DBF_DATE_LEN);
    break;
  }
  case 'L':
    if (v->type != XB_T_LOGICAL) return XB_PUT_TYPE;
    p[0] = v->v.l ? 'T' : 'F';
    break;
  case 'M': { /* a new memo; an empty one is none */
    if (v->type != XB_T_STRING) return XB_PUT_TYPE;
    uint32_t block = 0;
    if (v->v.s->len && !xb_dbt_append(&d->memo, v->v.s->data, v->v.s->len, &block, err)) return XB_PUT_FAILED;
    put_memo_block(p, block);
    break;
  }
  default: {
    if (!xb_is_number(v)) return XB_PUT_TYPE;
    const xb_str *s = xb_num_to_field(v, (int)fi->len, (int)fi->dec);
    memcpy(p, s->data, fi->len);
    if (s->data[0] == '*') r = XB_PUT_WIDTH; /* the stars stay, as in Clipper */
    break;
  }
  }
  d->dirty = d->changed = 1;
  return r;
}

static const unsigned char *dbf_field_bytes(xb_area *a, uint32_t field) {
  return state(a)->record + a->fields[field].offset;
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
    uint32_t block = memo_block((const char *)d->record + a->fields[k].offset);
    *blocks = 0;
    if (block) {
      xb_item text;
      if (!xb_dbt_read(&d->memo, block, &text, err)) return 0; /* unrooted: nothing allocates while it is used */
      if (text.v.s->len && !xb_dbt_append(&d->memo, text.v.s->data, text.v.s->len, blocks, err)) return 0;
    }
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
    uint32_t block = blocks ? *blocks++ : memo_block(p);
    put_memo_block(p, block ? block - shift : 0);
  }
}

/* PACK with memos goes in steps that each leave every record on a whole memo, so that a failure (a full disk) at any
 * of them loses nothing: the memos of the records kept are copied after every older memo; the records kept move to
 * the front with their copies' blocks; the copies move to the first block when they fit before themselves, the
 * records follow, and the older memos are cut. */
static int pack(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  uint32_t memos = 0, kept = 0, from = d->hasMemo ? d->memo.next : 0;
  for (uint32_t k = 0; k < a->nfields; ++k) memos += a->fields[k].type == 'M';
  if (memos && d->count > SIZE_MAX / sizeof(uint32_t) / memos) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  uint32_t *blocks = memos ? malloc(((size_t)d->count * memos + 1) * sizeof *blocks) : NULL;
  if (memos && !blocks) xb_gc_fatal("out of memory");
  int ok = 1;
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
  int moved = 0;
  if (memos && !xb_dbt_move_down(&d->memo, from, &moved, err)) return 0;
  for (uint32_t k = 1; moved && k <= d->count; ++k) {
    if (!xb_file_read_at(d->f, record_pos(d, k), d->record, d->recordLen, err)) return 0;
    set_memos(a, NULL, from - 1);
    if (!xb_platform_file_write_at(d->f, record_pos(d, k), d->record, d->recordLen, err)) return 0;
  }
  return !moved || xb_dbt_cut(&d->memo, from, err);
}

static int dbf_pack(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  d->aheadCount = 0;
  int ok = dbf_flush(a, 0, err) && pack(a, err);
  d->recno = d->count + 1; /* the record read last is no longer the current one */
  memset(d->record, ' ', d->recordLen);
  d->loaded = 1;
  return ok;
}

static int dbf_zap(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  d->aheadCount = 0;
  d->dirty = 0;
  d->count = 0;
  d->recno = 1; /* the blank record after the last */
  memset(d->record, ' ', d->recordLen);
  d->loaded = 1;
  return cut(d, err) && (!d->hasMemo || xb_dbt_zap(&d->memo, err));
}

static uint32_t dbf_header_size(xb_area *a) { return state(a)->headerLen; }
static uint32_t dbf_record_size(xb_area *a) { return state(a)->recordLen; }

static int64_t dbf_last_update(xb_area *a) {
  const unsigned char *h = state(a)->header;
  return xb_date_encode(1900 + h[1], h[2], h[3]);
}

const xb_rdd xb_rdd_dbf = {
    .name = "DBFNTX",
    .check = dbf_check,
    .create = dbf_create,
    .open = dbf_open,
    .close = dbf_close,
    .lastrec = dbf_lastrec,
    .go = dbf_go,
    .flush = dbf_flush,
    .append = dbf_append,
    .get = dbf_get,
    .put = dbf_put,
    .field_bytes = dbf_field_bytes,
    .load = dbf_load,
    .deleted = dbf_deleted,
    .set_deleted = dbf_set_deleted,
    .pack = dbf_pack,
    .zap = dbf_zap,
    .header_size = dbf_header_size,
    .record_size = dbf_record_size,
    .last_update = dbf_last_update,
};
