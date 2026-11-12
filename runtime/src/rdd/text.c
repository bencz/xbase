/* The text file drivers SDF and DELIM (docs/rdd.md#text-files): a record is a line, with the fields of a structure
 * the table side gives (a text file keeps none). SDF puts the fields' bytes side by side; DELIM writes each field's
 * text, character fields between delimiters, separated by a separator. A text file is created and written in record
 * order, or read; reading finds where its lines start once, then reads a line when its record is used.
 *
 * Lines end at the platform's newline when written; read, a line ends at CR or LF, the other one right after it
 * belonging to the same end. A Ctrl-Z (0x1A) ends a written file and is dropped wherever it is read. */
#include "rdd/rdd.h"

#include "core/file.h"
#include "core/numeric.h"

#include <stdlib.h>
#include <string.h>

#define TEXT_EOF 0x1A
#define TEXT_CHUNK 65536 /* bytes read or written at once */
#define TEXT_QUOTE '"'   /* DELIM's default delimiter */

typedef struct text {
  xb_file f;
  int writing;              /* created: records are added and written in order; else read */
  int delimited;            /* DELIM, else SDF */
  char delim, sep;          /* DELIM: the character fields' delimiter (0: none) and the fields' separator */
  uint32_t width;           /* the record's bytes: the fields' (a memo has none) */
  unsigned char *record;    /* a byte where a table keeps the deleted mark (unused), then the fields' */
  uint32_t recno, count;    /* the current record (count + 1: none), the records */
  int loaded, pending;      /* reading: the record's line was read; writing: the record is not written yet */
  uint64_t *starts;         /* reading: where each record's line starts */
  unsigned char *win;       /* reading: the file's bytes from winPos (winLen of them) */
  uint64_t winPos;
  size_t winLen;
  char *buf;                /* writing: lines not written yet; reading: a line */
  size_t used, cap;
} text;

static text *state(xb_area *a) { return a->state; }

static int is_memo(const xb_field_info *fi) { return fi->type == 'M'; }


static int text_create(const xb_rdd *rdd, const char *path, const xb_field_info *fields, uint32_t count,
                       int32_t *err) {
  XB_UNUSED(rdd);
  XB_UNUSED(fields);
  XB_UNUSED(count);
  xb_file f;
  return xb_platform_file_create(path, 0, &f, err) && xb_platform_file_close(f, err);
}

static void grow(text *t, size_t need) {
  if (need <= t->cap) return;
  size_t cap = t->cap ? t->cap : TEXT_CHUNK;
  while (cap < need) cap *= 2;
  char *b = realloc(t->buf, cap);
  if (!b) xb_gc_fatal("out of memory");
  t->buf = b;
  t->cap = cap;
}

static void put_bytes(text *t, const void *p, size_t n) {
  grow(t, t->used + n);
  memcpy(t->buf + t->used, p, n);
  t->used += n;
}

static int drain(text *t, int32_t *err) {
  if (!t->used) return 1;
  int ok = xb_platform_file_write(t->f, t->buf, t->used, err);
  t->used = 0;
  return ok;
}

/* The bytes of a field's text with the blanks at one end left out: leading (`lead`) or trailing ones. */
static void trimmed(const char *p, uint32_t len, int lead, const char **start, size_t *n) {
  size_t a = 0, b = len;
  if (lead) {
    while (a < b && p[a] == ' ') ++a;
  } else {
    while (b > a && p[b - 1] == ' ') --b;
  }
  *start = p + a;
  *n = b - a;
}

/* DELIM's text of a field: a character field's text without trailing blanks, between the delimiters; a number's
 * without leading blanks (a blank one is 0 with its decimals); a date's digits (none when empty); a logical's T or F.
 * Memos are not written, nor their separators. */
static void put_delimited(text *t, const xb_area *a) {
  int first = 1;
  for (uint32_t k = 0; k < a->nfields; ++k) {
    const xb_field_info *fi = &a->fields[k];
    if (is_memo(fi)) continue;
    if (!first) put_bytes(t, &t->sep, 1);
    first = 0;
    const char *p = (const char *)t->record + fi->offset, *s;
    size_t n;
    switch (fi->type) {
    case 'C':
      trimmed(p, fi->len, 0, &s, &n);
      if (t->delim) put_bytes(t, &t->delim, 1);
      put_bytes(t, s, n);
      if (t->delim) put_bytes(t, &t->delim, 1);
      break;
    case 'D':
      if (p[0] != ' ') put_bytes(t, p, fi->len);
      break;
    case 'L':
      put_bytes(t, xb_field_true(p[0]) ? "T" : "F", 1);
      break;
    default:
      trimmed(p, fi->len, 1, &s, &n);
      if (n) {
        put_bytes(t, s, n);
      } else {
        put_bytes(t, "0", 1);
        if (fi->dec) put_bytes(t, ".", 1);
        for (uint32_t d = 0; d < fi->dec; ++d) put_bytes(t, "0", 1);
      }
    }
  }
}

/* Writes the current record's line when it is not written yet. */
static int write_pending(text *t, const xb_area *a, int32_t *err) {
  if (!t->pending) return 1;
  t->pending = 0;
  if (t->delimited) put_delimited(t, a);
  else put_bytes(t, t->record + 1, t->width);
  const char *eol = xb_platform_eol();
  put_bytes(t, eol, strlen(eol));
  return t->used < TEXT_CHUNK || drain(t, err);
}

/* The record past the last: blank. */
static void blank(text *t) {
  memset(t->record, ' ', t->width + 1);
  t->recno = t->count + 1;
  t->loaded = 1;
}

/* Keeps where a line starts: 0 when there are more lines than records can be numbered (32 bits). */
static int add_start(text *t, size_t *cap, uint64_t start) {
  if (t->count == UINT32_MAX - 1) return 0;
  if (t->count == *cap) {
    *cap = *cap ? *cap * 2 : 1024;
    uint64_t *grown = realloc(t->starts, *cap * sizeof *grown);
    if (!grown) xb_gc_fatal("out of memory");
    t->starts = grown;
  }
  t->starts[t->count++] = start;
  return 1;
}

/* Finds where the lines start: a line ends at CR or LF (the other one right after it is the same end); a line with
 * nothing but the end is a record, the end of the file without a byte since the last end is none; Ctrl-Z counts as no
 * byte. */
static int index_lines(text *t, int32_t *err) {
  unsigned char *chunk = malloc(TEXT_CHUNK);
  if (!chunk) xb_gc_fatal("out of memory");
  size_t cap = 0;
  uint64_t base = 0, start = 0;
  int content = 0, ended = 0; /* bytes since the line started; the last byte ended a line (its CR or LF) */
  unsigned char endByte = 0;
  for (;;) {
    int64_t got = xb_file_read_up_to(t->f, chunk, TEXT_CHUNK, err);
    if (got < 0) {
      free(chunk);
      return 0;
    }
    const unsigned char *p = chunk, *end = chunk + got;
    while (p < end) {
      if (ended && (*p == '\r' || *p == '\n') && *p != endByte) { /* CR LF or LF CR: one end */
        ended = 0;
        start = base + (uint64_t)(++p - chunk);
        continue;
      }
      ended = 0;
      const unsigned char *lf = memchr(p, '\n', (size_t)(end - p));
      const unsigned char *cr = memchr(p, '\r', (size_t)((lf ? lf : end) - p));
      const unsigned char *q = cr ? cr : lf ? lf : end;
      for (const unsigned char *r = p; r < q && !content; ++r) content = *r != TEXT_EOF;
      if (q == end) break;
      if (!add_start(t, &cap, start)) {
        free(chunk);
        *err = XB_FERR_FAILURE;
        return 0;
      }
      endByte = *q;
      ended = 1;
      content = 0;
      p = q + 1;
      start = base + (uint64_t)(p - chunk);
    }
    base += (uint64_t)got;
    if (got < TEXT_CHUNK) break;
  }
  free(chunk);
  if (content && !add_start(t, &cap, start)) {
    *err = XB_FERR_FAILURE;
    return 0;
  }
  return 1;
}

/* Reads the line that starts at `pos` into the line buffer, Ctrl-Z dropped, without its end; the file is read a
 * chunk at a time, records being read in order mostly. */
static int read_line(text *t, uint64_t pos, int32_t *err) {
  t->used = 0;
  for (;;) {
    if (pos < t->winPos || pos >= t->winPos + t->winLen) {
      int64_t got = xb_platform_file_read_at(t->f, (int64_t)pos, t->win, TEXT_CHUNK, err);
      if (got < 0) return 0;
      t->winPos = pos;
      t->winLen = (size_t)got;
      if (got == 0) return 1;
    }
    const unsigned char *p = t->win + (pos - t->winPos), *end = t->win + t->winLen;
    const unsigned char *lf = memchr(p, '\n', (size_t)(end - p));
    const unsigned char *cr = memchr(p, '\r', (size_t)((lf ? lf : end) - p));
    const unsigned char *q = cr ? cr : lf ? lf : end;
    size_t n = (size_t)(q - p);
    grow(t, t->used + n);
    if (!memchr(p, TEXT_EOF, n)) {
      memcpy(t->buf + t->used, p, n);
      t->used += n;
    } else {
      for (const unsigned char *r = p; r < q; ++r)
        if (*r != TEXT_EOF) t->buf[t->used++] = (char)*r;
    }
    if (q < end) return 1;
    pos += (uint64_t)(q - p);
  }
}

/* A number's text written as its field stores it, right-aligned, when it already has the field's decimals and fits:
 * 0 otherwise. */
static int place_number(const xb_field_info *fi, const char *s, size_t n, char *out) {
  if (n > fi->len || !xb_field_number_text(s, n, fi->dec)) return 0;
  memset(out, ' ', fi->len - n);
  memcpy(out + fi->len - n, s, n);
  return 1;
}

/* DELIM's fields from a line: each one's text up to the separator, or, starting with the delimiter (blanks before it
 * skipped), up to the next delimiter, what follows it up to the separator dropped. A character field takes the text
 * (cut to its length); a number its value, in the field's form (stars when it does not fit); a date eight digits of a
 * date, blanks after them aside (else none); a logical T when the text starts with T or Y, else F. Fields the line
 * does not reach stay blank; memos take nothing. */
static void get_delimited(text *t, const xb_area *a) {
  const char *p = t->buf, *end = t->buf + t->used;
  int present = t->used > 0; /* the line reaches the field: it is not empty, or a separator came before the field */
  for (uint32_t k = 0; k < a->nfields && present; ++k) {
    const xb_field_info *fi = &a->fields[k];
    if (is_memo(fi)) continue;
    while (p < end && *p == ' ') ++p; /* so runs of blanks separate the fields of a blank-separated line */
    const char *s, *e;
    if (t->delim && p < end && *p == t->delim) {
      s = ++p;
      while (p < end && *p != t->delim) ++p;
      e = p;
      while (p < end && *p != t->sep) ++p;
    } else {
      s = p;
      while (p < end && *p != t->sep) ++p;
      e = p;
    }
    present = p < end; /* a separator follows */
    if (present) ++p;
    char *out = (char *)t->record + fi->offset;
    size_t n = (size_t)(e - s);
    switch (fi->type) {
    case 'C':
      memcpy(out, s, n < fi->len ? n : fi->len);
      break;
    case 'L':
      out[0] = n && xb_field_true(s[0]) ? 'T' : 'F';
      break;
    case 'D': {
      while (n && s[n - 1] == ' ') --n;
      xb_item d;
      xb_set_date(&d, n == fi->len ? xb_date_from_digits(s) : 0);
      xb_field_encode(fi, out, &d);
      break;
    }
    default:
      while (n && s[n - 1] == ' ') --n;
      if (!place_number(fi, s, n, out)) {
        xb_item v;
        xb_num_parse(s, n, &v);
        xb_field_encode(fi, out, &v);
      }
    }
  }
}

/* Reads the current record's line, when it is not read yet. */
static int text_load(xb_area *a, int32_t *err) {
  text *t = state(a);
  if (t->loaded || t->writing) return 1;
  t->loaded = 1;
  memset(t->record, ' ', t->width + 1);
  if (!read_line(t, t->starts[t->recno - 1], err)) {
    blank(t);
    return 0;
  }
  if (t->delimited) get_delimited(t, a);
  else memcpy(t->record + 1, t->buf, t->used < t->width ? t->used : t->width);
  return 1;
}

/* DELIM's delimiter and separator from the value given: NIL a quote and a comma; "BLANK", "TAB", "PIPE" (any letter
 * case) no delimiter and a space, tab or bar; another string its first character (none for ""), with a comma; an
 * array { cDelimiter, cSeparator } each element's first character (the separator stays a comma when it is empty). */
static void delimiters(text *t, const xb_item *v) {
  t->delim = TEXT_QUOTE;
  t->sep = ',';
  if (!v) return;
  if (v->type == XB_T_STRING) {
    static const struct {
      const char *word;
      char sep;
    } words[] = {{"BLANK", ' '}, {"TAB", '\t'}, {"PIPE", '|'}};
    const xb_str *s = v->v.s;
    for (size_t k = 0; k < sizeof words / sizeof words[0]; ++k) {
      size_t i = 0;
      while (i < s->len && words[k].word[i] && xb_ascii_upper(s->data[i]) == words[k].word[i]) ++i;
      if (i == s->len && words[k].word[i] == '\0') {
        t->delim = 0;
        t->sep = words[k].sep;
        return;
      }
    }
    t->delim = s->len ? s->data[0] : 0;
  } else if (v->type == XB_T_ARRAY) {
    const xb_array *arr = v->v.p;
    if (arr->len > 0 && arr->items[0].type == XB_T_STRING)
      t->delim = arr->items[0].v.s->len ? arr->items[0].v.s->data[0] : 0;
    if (arr->len > 1 && arr->items[1].type == XB_T_STRING && arr->items[1].v.s->len)
      t->sep = arr->items[1].v.s->data[0];
  }
}

static int text_open_with(xb_area *a, const char *path, const xb_field_info *fields, uint32_t count, int create,
                          const xb_item *delimiter, xb_open_failure *why) {
  text *t = calloc(1, sizeof *t);
  xb_field_info *own = malloc(count * sizeof *own);
  if (!t || !own) xb_gc_fatal("out of memory");
  memcpy(own, fields, count * sizeof *own);
  for (uint32_t k = 0; k < count; ++k) {
    own[k].offset = 1 + t->width; /* as a table's, past the deleted mark */
    if (!is_memo(&own[k])) t->width += own[k].len;
  }
  t->record = malloc(t->width + 1);
  if (!t->record) xb_gc_fatal("out of memory");
  t->writing = create;
  t->delimited = a->rdd->delimited;
  delimiters(t, delimiter);
  int ok = create ? xb_platform_file_create(path, 0, &t->f, &why->oscode)
                  : xb_platform_file_open(path, FO_READ | FO_SHARED, &t->f, &why->oscode);
  if (ok && !create) {
    t->win = malloc(TEXT_CHUNK);
    if (!t->win) xb_gc_fatal("out of memory");
  }
  if (ok && !create && !index_lines(t, &why->oscode)) {
    int32_t ignored;
    xb_platform_file_close(t->f, &ignored);
    ok = 0;
  }
  if (!ok) {
    free(t->starts);
    free(t->win);
    free(t->record);
    free(t);
    free(own);
    return 0;
  }
  blank(t);
  a->state = t;
  a->fields = own;
  a->nfields = count;
  return 1;
}

/* A text file is opened only with a structure (text_open_with). */
static int text_open(xb_area *a, const char *path, int readonly, int exclusive, xb_open_failure *why) {
  XB_UNUSED(a);
  XB_UNUSED(path);
  XB_UNUSED(readonly);
  XB_UNUSED(exclusive);
  why->oscode = XB_FERR_ACCESS;
  return 0;
}

static int text_close(xb_area *a, int32_t *err) {
  text *t = state(a);
  int ok = 1;
  if (t->writing) {
    unsigned char mark = TEXT_EOF;
    ok = write_pending(t, a, err);
    if (ok) put_bytes(t, &mark, 1);
    ok = ok && drain(t, err);
  }
  int32_t closeErr = 0;
  if (!xb_platform_file_close(t->f, &closeErr) && ok) {
    *err = closeErr;
    ok = 0;
  }
  free(t->starts);
  free(t->win);
  free(t->buf);
  free(t->record);
  free(t);
  free(a->fields);
  a->fields = NULL;
  a->state = NULL;
  return ok;
}

static uint32_t text_lastrec(xb_area *a) { return state(a)->count; }

static int text_go(xb_area *a, uint32_t recno, int32_t *err) {
  text *t = state(a);
  if (t->writing && !write_pending(t, a, err)) return 0;
  if (t->writing || recno < 1 || recno > t->count) {
    blank(t);
    return 1;
  }
  t->recno = recno;
  t->loaded = 0;
  return 1;
}

static int text_flush(xb_area *a, int header, int32_t *err) {
  XB_UNUSED(header);
  text *t = state(a);
  return !t->writing || (write_pending(t, a, err) && drain(t, err));
}

static int text_append(xb_area *a, uint32_t *recno, int32_t *err) {
  text *t = state(a);
  if (!t->writing) {
    *err = XB_FERR_ACCESS;
    return 0;
  }
  if (!write_pending(t, a, err)) return 0;
  memset(t->record, ' ', t->width + 1);
  *recno = t->recno = ++t->count;
  t->pending = 1;
  return 1;
}

static int text_lock(xb_area *a, uint32_t recno, int on, int32_t *err) {
  XB_UNUSED(a);
  XB_UNUSED(recno);
  XB_UNUSED(on);
  XB_UNUSED(err);
  return 1;
}

static int text_get(xb_area *a, uint32_t field, xb_item *r, int32_t *err) {
  if (!text_load(a, err)) return 0;
  const xb_field_info *fi = &a->fields[field];
  if (is_memo(fi)) xb_set_str(r, xb_str_empty());
  else xb_field_decode(fi, (const char *)state(a)->record + fi->offset, r);
  return 1;
}

static int text_put(xb_area *a, uint32_t field, const xb_item *v, int32_t *err) {
  XB_UNUSED(err);
  const xb_field_info *fi = &a->fields[field];
  if (is_memo(fi)) return v->type == XB_T_STRING ? XB_PUT_OK : XB_PUT_TYPE;
  return xb_field_encode(fi, (char *)state(a)->record + fi->offset, v);
}

static const unsigned char *text_field_bytes(xb_area *a, uint32_t field) {
  return state(a)->record + a->fields[field].offset;
}

static const unsigned char *text_record_bytes(xb_area *a) { return state(a)->record + 1; }

static int text_put_record_bytes(xb_area *a, const unsigned char *bytes, int32_t *err) {
  XB_UNUSED(err);
  text *t = state(a);
  memcpy(t->record + 1, bytes, t->width);
  return 1;
}

static void text_refresh(xb_area *a) { XB_UNUSED(a); }
static int text_deleted(xb_area *a) {
  XB_UNUSED(a);
  return 0;
}
static void text_set_deleted(xb_area *a, int deleted) {
  XB_UNUSED(a);
  XB_UNUSED(deleted);
}

/* PACK, ZAP: a text file has no deleted records to remove, and is not emptied. */
static int text_refuse(xb_area *a, int32_t *err) {
  XB_UNUSED(a);
  *err = XB_FERR_ACCESS;
  return 0;
}

static uint32_t text_header_size(xb_area *a) {
  XB_UNUSED(a);
  return 0;
}
static uint32_t text_record_size(xb_area *a) { return state(a)->width + 1; }
static int64_t text_last_update(xb_area *a) {
  XB_UNUSED(a);
  return 0;
}
static int text_set_production(xb_area *a, int on, int32_t *err) {
  XB_UNUSED(a);
  XB_UNUSED(on);
  XB_UNUSED(err);
  return 1;
}

#define TEXT_DRIVER(NAME, DELIMITED)                                                                                  \
  {.name = NAME,                                                                                                      \
   .ext = ".txt",                                                                                                     \
   .text = 1,                                                                                                         \
   .delimited = DELIMITED,                                                                                            \
   .lockScheme = XB_LOCKS_CLIPPER,                                                                                    \
   .check = xb_dbf_check,                                                                                             \
   .create = text_create,                                                                                             \
   .open = text_open,                                                                                                 \
   .open_with = text_open_with,                                                                                       \
   .close = text_close,                                                                                               \
   .lastrec = text_lastrec,                                                                                           \
   .go = text_go,                                                                                                     \
   .flush = text_flush,                                                                                               \
   .append = text_append,                                                                                             \
   .lock = text_lock,                                                                                                 \
   .get = text_get,                                                                                                   \
   .put = text_put,                                                                                                   \
   .field_bytes = text_field_bytes,                                                                                   \
   .record_bytes = text_record_bytes,                                                                                 \
   .put_record_bytes = text_put_record_bytes,                                                                         \
   .load = text_load,                                                                                                 \
   .refresh = text_refresh,                                                                                           \
   .deleted = text_deleted,                                                                                           \
   .set_deleted = text_set_deleted,                                                                                   \
   .pack = text_refuse,                                                                                               \
   .zap = text_refuse,                                                                                                \
   .header_size = text_header_size,                                                                                   \
   .record_size = text_record_size,                                                                                   \
   .last_update = text_last_update,                                                                                   \
   .set_production = text_set_production}

const xb_rdd xb_rdd_sdf = TEXT_DRIVER("SDF", 0);
const xb_rdd xb_rdd_delim = TEXT_DRIVER("DELIM", 1);
