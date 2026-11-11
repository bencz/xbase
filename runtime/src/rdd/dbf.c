/* The DBF table (dBASE III, as Clipper writes it). Every binary field is little-endian (core/bytes.h):
 *
 *   header, 32 bytes: 0 version (0x03), 1-3 the date of the last update (year - 1900, month, day), 4-7 the record
 *     count, 8-9 the header's length, 10-11 the record's length, 29 the codepage byte, the rest zero;
 *   a 32-byte descriptor per field: 0-10 the name (NUL-padded), 11 the type, 16 the length, 17 the decimals (a
 *     character field's length takes both bytes, the decimals byte its high byte), the rest zero;
 *   0x0D and 0x00 after the descriptors (the header's length counts both);
 *   the records, each a deletion flag (' ' or '*') and the fields' text, back to back;
 *   0x1A after the last record. */
#include "rdd/rdd.h"

#include "core/bytes.h"
#include "core/file.h"
#include "core/numeric.h"

#include <stdlib.h>
#include <string.h>

#define DBF_HEADER 32
#define DBF_FIELD 32
#define DBF_VERSION 0x03
#define DBF_FIELDS_END 0x0D
#define DBF_EOF 0x1A
#define DBF_DELETED '*'
#define DBF_MAX_FIELDS 2046      /* descriptors that fit the 16-bit header length */
#define DBF_MAX_NUMBER 255       /* the length byte of a number */
#define DBF_DATE_LEN 8           /* YYYYMMDD */

typedef struct dbf {
  xb_file f;
  uint32_t count;          /* records */
  uint32_t headerLen, recordLen;
  unsigned char header[DBF_HEADER];
  unsigned char *record;   /* the current record */
  uint32_t recno;          /* its number (count + 1: the blank record after the last) */
  int dirty;               /* the record changed since it was read */
  int changed;             /* the table changed: the header gets the date and count */
} dbf;

static dbf *state(xb_area *a) { return a->state; }

static int64_t record_pos(const dbf *d, uint32_t recno) {
  return (int64_t)d->headerLen + (int64_t)(recno - 1) * d->recordLen;
}

static int write_at(xb_file f, int64_t pos, const void *p, size_t n, int32_t *err) {
  return xb_platform_file_seek(f, pos, FS_SET, err) == pos && xb_platform_file_write(f, p, n, err);
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
  default: return 0; /* memo and other types come with their drivers */
  }
}

/* Whether a field of another program's table can be read and written: a known type, at least a byte, a whole date. */
static int field_readable(char type, uint32_t len) {
  return strchr("CNFDL", type) && type && len >= 1 && (type != 'D' || len == DBF_DATE_LEN);
}

static int dbf_check(xb_field_info *fields, uint32_t count) {
  uint32_t recordLen = 1;
  if (count > DBF_MAX_FIELDS) return 0;
  for (uint32_t k = 0; k < count; ++k) {
    xb_field_info *fi = &fields[k];
    if (fi->type == 'C' || fi->type == 'D' || fi->type == 'L') fi->dec = 0;
    if (fi->type == 'D') fi->len = DBF_DATE_LEN;
    if (fi->type == 'L') fi->len = 1;
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
  h[0] = DBF_VERSION;
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
  return ok;
}

/* Reads `n` bytes at `pos`: whether all of them were there (a file that ends early is a general failure). */
static int read_at(xb_file f, int64_t pos, unsigned char *p, size_t n, int32_t *err) {
  if (xb_platform_file_seek(f, pos, FS_SET, err) != pos) return 0;
  int64_t got = xb_file_read_up_to(f, p, n, err);
  if (got >= 0 && (size_t)got < n) *err = XB_FERR_FAILURE;
  return got >= 0 && (size_t)got == n;
}

static int dbf_open(xb_area *a, const char *path, int readonly, int32_t *err) {
  dbf *d = calloc(1, sizeof *d);
  if (!d) xb_gc_fatal("out of memory");
  if (!xb_platform_file_open(path, readonly ? FO_READ : FO_READWRITE, &d->f, err)) {
    free(d);
    return 0;
  }
  unsigned char *h = NULL;
  xb_field_info *fields = NULL;
  uint32_t n = 0;
  if (!read_at(d->f, 0, d->header, DBF_HEADER, err)) goto fail;
  d->count = xb_get_le32(d->header + 4);
  d->headerLen = xb_get_le16(d->header + 8);
  d->recordLen = xb_get_le16(d->header + 10);
  if (d->headerLen < DBF_HEADER + 1 || d->recordLen < 1) goto corrupt;
  h = malloc(d->headerLen);
  fields = malloc(((d->headerLen - DBF_HEADER) / DBF_FIELD + 1) * sizeof *fields);
  if (!h || !fields) xb_gc_fatal("out of memory");
  if (!read_at(d->f, 0, h, d->headerLen, err)) goto fail;
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
  d->record = malloc(d->recordLen);
  if (!d->record) xb_gc_fatal("out of memory");
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
  if (!write_at(d->f, 0, d->header, DBF_HEADER, err)) return 0;
  d->changed = 0;
  return 1;
}

static int dbf_flush(xb_area *a, int header, int32_t *err) {
  dbf *d = state(a);
  if (d->dirty) {
    if (!write_at(d->f, record_pos(d, d->recno), d->record, d->recordLen, err)) return 0;
    if (d->recno == d->count) { /* the last record: the end mark after it */
      unsigned char eof = DBF_EOF;
      if (!xb_platform_file_write(d->f, &eof, 1, err)) return 0;
    }
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
  free(d->record);
  free(d);
  free(a->fields);
  a->state = NULL;
  a->fields = NULL;
  return ok;
}

static uint32_t dbf_lastrec(xb_area *a) { return state(a)->count; }

static int dbf_go(xb_area *a, uint32_t recno, int32_t *err) {
  dbf *d = state(a);
  if (recno >= 1 && recno <= d->count && read_at(d->f, record_pos(d, recno), d->record, d->recordLen, err)) {
    d->recno = recno;
    return 1;
  }
  d->recno = d->count + 1; /* the blank record after the last, also when the record cannot be read */
  memset(d->record, ' ', d->recordLen);
  return recno < 1 || recno > d->count;
}

static int dbf_append(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  if (!dbf_flush(a, 0, err)) return 0;
  d->recno = ++d->count;
  memset(d->record, ' ', d->recordLen);
  d->dirty = d->changed = 1;
  return 1;
}

static void dbf_get(xb_area *a, uint32_t field, xb_item *r) {
  const dbf *d = state(a);
  const xb_field_info *fi = &a->fields[field];
  const char *p = (const char *)d->record + fi->offset;
  switch (fi->type) {
  case 'C':
    xb_set_str(r, xb_str_new(p, fi->len));
    return;
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
    return;
  }
  case 'L':
    xb_set_logical(r, p[0] && strchr("TtYy", p[0]));
    return;
  default: { /* N, F: the text's number, with the field's width and decimals */
    xb_num_parse(p, fi->len, r);
    if (fi->dec == 0 && r->type == XB_T_INT) {
      xb_put(r, xb_header_word(XB_T_INT, (uint16_t)fi->len, 0), (uint64_t)r->v.i);
      return;
    }
    double v = r->type == XB_T_INT ? (double)r->v.i : r->v.d;
    uint64_t bits;
    memcpy(&bits, &v, 8);
    uint32_t width = fi->dec ? (fi->len > fi->dec + 1 ? fi->len - fi->dec - 1 : 1) : fi->len;
    xb_put(r, xb_header_word(XB_T_DOUBLE, (uint16_t)width, (uint16_t)fi->dec), bits);
    return;
  }
  }
}

static int dbf_put(xb_area *a, uint32_t field, const xb_item *v) {
  dbf *d = state(a);
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
    int y, m, day;
    if (v->v.i == 0) {
      memset(p, ' ', fi->len);
      break;
    }
    xb_date_decode(v->v.i, &y, &m, &day);
    int digits[DBF_DATE_LEN] = {y / 1000 % 10, y / 100 % 10, y / 10 % 10, y % 10, m / 10, m % 10, day / 10, day % 10};
    for (int i = 0; i < DBF_DATE_LEN; ++i) p[i] = (char)('0' + digits[i]);
    break;
  }
  case 'L':
    if (v->type != XB_T_LOGICAL) return XB_PUT_TYPE;
    p[0] = v->v.l ? 'T' : 'F';
    break;
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
  return xb_platform_file_truncate(d->f, end, err) && write_at(d->f, end, &eof, 1, err);
}

static int dbf_pack(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  if (!dbf_flush(a, 0, err)) return 0;
  uint32_t kept = 0;
  for (uint32_t k = 1; k <= d->count; ++k) {
    if (!read_at(d->f, record_pos(d, k), d->record, d->recordLen, err)) return 0;
    if (d->record[0] == DBF_DELETED) continue;
    if (++kept != k && !write_at(d->f, record_pos(d, kept), d->record, d->recordLen, err)) return 0;
  }
  d->count = kept;
  return cut(d, err);
}

static int dbf_zap(xb_area *a, int32_t *err) {
  dbf *d = state(a);
  d->dirty = 0;
  d->count = 0;
  return cut(d, err);
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
    .deleted = dbf_deleted,
    .set_deleted = dbf_set_deleted,
    .pack = dbf_pack,
    .zap = dbf_zap,
    .header_size = dbf_header_size,
    .record_size = dbf_record_size,
    .last_update = dbf_last_update,
};
