/* The primitives of the copies between tables and text files (docs/rdd.md#copies-between-files), which the commands'
 * functions in rdd/transfer.prg use: __dbTransOpen opens the other file in an area of its own, __dbTrans copies the
 * records of a scope. */
#include "rdd/rdd.h"

#include "core/file.h"
#include "core/numeric.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* __dbUsed( cOperation ): whether the current area has a table; without one, "Workarea not in use" (DBCMD/2001) for
 * the operation (the command's name). */
void xbf___DBUSED(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *op = xb_db_str_arg(argc, argv, 0);
  char name[32];
  snprintf(name, sizeof name, "%.*s", op ? (int)(op->len < 31 ? op->len : 31) : 0, op ? op->data : "");
  xb_set_logical(ret, xb_db_used(ctx, name) != NULL);
}

/* Opens the other file of a copy in the lowest free area (the current area stays current; the driver's extension
 * added to a name without one): created (a table or a text file of the fields), opened exclusive; else a table opened
 * shared and read-only, or a text file read with the fields. Its number, 0 after an error (raised). */
static uint32_t open_other(xb_ctx *ctx, const xb_str *name, const xb_rdd *rdd, int create, xb_field_info *fields,
                           uint32_t count, const xb_item *delimiter) {
  char *path = xb_file_path_with_ext(name->data, name->len, rdd->ext);
  uint32_t n = xb_area_free(ctx), before = ctx->env->areaCurrent;
  xb_text_open text = {fields, count, create, delimiter};
  int ok;
  if (rdd->text) ok = xb_area_open_with(ctx, n, rdd, path, !create, create, &text);
  else ok = (!create || xb_db_create(ctx, rdd, path, fields, count)) &&
            xb_area_open_with(ctx, n, rdd, path, !create, create, NULL);
  ctx->env->areaCurrent = before;
  free(path);
  return ok ? n : 0;
}

/* The driver of argument `k` and the fields of a file to make, or of a text file to read (a copy of `fields`, checked
 * by the driver): 1, or 0 after the error (raised): a driver of no name DBCMD/1015, fields it cannot make 1014. */
static int new_file(xb_ctx *ctx, const char *fn, uint32_t argc, xb_item *argv, uint32_t k, const xb_field_info *fields,
                    uint32_t count, const xb_rdd **rdd, xb_field_info **own) {
  *rdd = xb_db_driver_arg(ctx, argc, argv, k);
  *own = malloc((count ? count : 1) * sizeof **own);
  if (!*own) xb_gc_fatal("out of memory");
  if (count) memcpy(*own, fields, count * sizeof **own);
  if (*rdd && count && (*rdd)->check(*rdd, *own, count)) return 1;
  free(*own);
  *own = NULL;
  xb_error_of(ctx, XB_DBCMD, EG_ARG, *rdd ? 1014 : 1015, fn, NULL, 0, 0, argc, argv, 0, NULL);
  return 0;
}

/* __dbTransOpen( cFile, [cDriver], lCreate, [aStruct], [xDelimiter] ): the number of a new area holding the other
 * file of a copy (open_other; aStruct: the fields of a file created or of a text file), 0 after an error (raised). A
 * driver of no name is DBCMD/1015, a structure it cannot make 1014. */
void xbf___DBTRANSOPEN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_int(ret, 0);
  const xb_str *name = xb_db_str_arg(argc, argv, 0);
  const xb_rdd *rdd = xb_db_driver_arg(ctx, argc, argv, 1);
  int create = xb_db_logical_arg(argc, argv, 2);
  xb_field_info *given = NULL, *fields = NULL;
  uint32_t count = 0;
  if (rdd && !rdd->text && !create) { /* a table keeps its structure */
    if (name) xb_set_int(ret, open_other(ctx, name, rdd, 0, NULL, 0, NULL));
    else xb_error_of(ctx, XB_DBCMD, EG_ARG, 1014, "__DBTRANSOPEN", NULL, 0, 0, argc, argv, 0, NULL);
    return;
  }
  xb_db_struct(argc > 3 ? &argv[3] : NULL, &given, &count);
  if (new_file(ctx, "__DBTRANSOPEN", argc, argv, 1, given, name ? count : 0, &rdd, &fields))
    xb_set_int(ret, open_other(ctx, name, rdd, create, fields, count, argc > 4 ? &argv[4] : NULL));
  free(given);
  free(fields);
}

/* How copy_fields takes a pair of fields: as values; of the same kind and size (bytes in the stored form copied); two
 * number fields of other sizes (a text file's stars kept). */
enum { PAIR_VALUE, PAIR_SAME, PAIR_NUMBERS };

/* What a copy takes from each record: the pairs of fields (source, target) of the same name, and whether the
 * records' bytes are copied whole (the same fields in the same places, no memo: the bytes as they are, blanks and
 * stars kept). From a text file, a field the line leaves blank stays blank, a number too wide for it stays stars;
 * others are read as the table's own. */
typedef struct transfer {
  xb_area *source, *target;
  uint32_t *from, *to, *later, count; /* later: copy_fields's fields written as values */
  unsigned char *pair;                /* each pair's PAIR_ */
  unsigned char *bytes;               /* copy_fields's record of the target */
  int whole, text;
  int deletedToo; /* a deleted record's copy is deleted (SORT's are not) */
  uint32_t last; /* the source's records when the copy started: records it appends to itself are not copied again */
} transfer;

/* Whether the `len` bytes at `p` are all `c`. */
static int all_bytes(const unsigned char *p, uint32_t len, unsigned char c) {
  for (uint32_t i = 0; i < len; ++i)
    if (p[i] != c) return 0;
  return 1;
}

/* Whether a field's bytes are what a table's field of its kind stores for a value: text, a number right-aligned with
 * its decimals, a date's digits, T or F (not blanks, nor stars). */
static int stored_form(const xb_field_info *fi, const unsigned char *p) {
  uint32_t i = 0;
  switch (fi->type) {
  case 'C':
    return 1;
  case 'L':
    return p[0] == 'T' || p[0] == 'F';
  case 'D':
    return xb_date_digits_valid((const char *)p);
  default:
    while (i < fi->len && p[i] == ' ') ++i;
    return xb_field_number_text((const char *)p + i, fi->len - i, fi->dec);
  }
}

/* The fields of the source's record into the target's new one: from a text file, a blank field stays blank and stars
 * stay stars in a number field; a field in the target's stored form, of the same kind and size, is copied as it is;
 * others are read and written as values (memos; fields of another kind or size; a table's blank numbers and
 * logicals, which become 0 and .F.). 0 after an error (raised). */
static int copy_fields(xb_ctx *ctx, transfer *t, xb_area *src, xb_area *dst) {
  unsigned char *bytes = t->bytes;
  const unsigned char *from = src->rdd->record_bytes(src);
  memcpy(bytes, dst->rdd->record_bytes(dst), dst->rdd->record_size(dst) - 1);
  uint32_t later = 0; /* the fields written as values come after the bytes */
  for (uint32_t k = 0; k < t->count; ++k) {
    const xb_field_info *fi = &src->fields[t->from[k]], *to = &dst->fields[t->to[k]];
    const unsigned char *p = from + fi->offset - 1;
    unsigned char *q = bytes + to->offset - 1;
    switch (t->pair[k]) {
    case PAIR_SAME:
      if (fi->type == 'C' || stored_form(fi, p)) {
        memcpy(q, p, fi->len);
        continue;
      }
      break;
    case PAIR_NUMBERS:
      break;
    default:
      if (fi->type == 'M' && t->text) continue; /* a text file's memos are none */
      t->later[later++] = k;
      continue;
    }
    if (t->text && all_bytes(p, fi->len, ' ')) continue;
    if (t->text && fi->type != 'D' && fi->type != 'L' && all_bytes(p, fi->len, '*')) memset(q, '*', to->len);
    else t->later[later++] = k;
  }
  int32_t err = 0;
  int ok = dst->rdd->put_record_bytes(dst, bytes, &err);
  if (!ok) xb_area_error(ctx, dst, EG_WRITE, 1011, "", err);
  for (uint32_t i = 0; ok && i < later; ++i) {
    uint32_t k = t->later[i];
    xb_item v;
    xb_set_nil(&v);
    xb_gc_root_push(ctx, &v);
    ok = xb_area_field_get(ctx, src, t->from[k], &v) && !src->closed && !dst->closed;
    ok = ok && (xb_area_field_put(ctx, dst, t->to[k], &v) || !(xb_pending_get(ctx) && xb_pending(ctx))) &&
         !dst->closed && !src->closed; /* the error block may close either */
    xb_gc_root_pop(ctx, 1);
  }
  return ok;
}

/* Copies the current record of `src` to a new record of the target; one whose copy failed (an error the error block
 * did not default) stays, deleted, and the copy ends (-1). */
static int copy_record(xb_ctx *ctx, xb_area *src, void *arg) {
  transfer *t = arg;
  xb_area *dst = t->target;
  if (src->recno > t->last) return 1;
  if (!xb_area_load(ctx, src) || src->closed) return -1;
  if (!xb_area_append(ctx, dst, 1) || dst->closed || src->closed) return -1; /* code it ran may close them */
  int ok = 1;
  int32_t err = 0;
  if (t->whole) {
    ok = dst->rdd->put_record_bytes(dst, src->rdd->record_bytes(src), &err);
    if (!ok) xb_area_error(ctx, dst, EG_WRITE, 1011, "", err);
  } else {
    ok = copy_fields(ctx, t, src, dst);
  }
  if (!dst->closed && (!ok || (t->deletedToo && !src->closed && src->rdd->deleted(src))))
    dst->rdd->set_deleted(dst, 1);
  return ok ? 1 : -1;
}

/* A copy from `src` into `dst` of the fields `list` names (names, or rows of a structure; NULL: every field of the
 * target) that both have, by name: its plan in `t` (transfer_end frees it). The source is pinned and read in order
 * until transfer_end. */
static void transfer_begin(transfer *t, xb_area *src, xb_area *dst, const xb_array *list) {
  memset(t, 0, sizeof *t);
  t->source = src;
  t->target = dst;
  t->text = src->rdd->text;
  t->deletedToo = 1;
  t->last = src->rdd->lastrec(src);
  size_t cap = list ? (list->len ? list->len : 1) : (dst->nfields ? dst->nfields : 1);
  t->from = malloc(cap * sizeof *t->from);
  t->to = malloc(cap * sizeof *t->to);
  t->later = malloc(cap * sizeof *t->later);
  t->bytes = malloc(dst->rdd->record_size(dst));
  t->pair = malloc(cap);
  if (!t->from || !t->to || !t->later || !t->bytes || !t->pair) xb_gc_fatal("out of memory");
  for (size_t k = 0; k < (list ? list->len : dst->nfields); ++k) {
    int64_t from, to;
    if (list) {
      const xb_item *it = &list->items[k];
      const xb_array *row = it->type == XB_T_ARRAY ? it->v.p : NULL;
      if (row && row->len > 0) it = &row->items[0];
      if (it->type != XB_T_STRING) continue;
      from = xb_area_field_index(src, it->v.s->data, it->v.s->len);
      to = xb_area_field_index(dst, it->v.s->data, it->v.s->len);
    } else {
      to = (int64_t)k;
      from = xb_area_field_index(src, dst->fields[k].name, strlen(dst->fields[k].name));
    }
    if (from < 0 || to < 0) continue;
    t->from[t->count] = (uint32_t)from;
    t->to[t->count++] = (uint32_t)to;
  }
  int whole = !src->rdd->text && t->count == src->nfields && t->count == dst->nfields;
  for (uint32_t k = 0; whole && k < t->count; ++k) {
    const xb_field_info *a = &src->fields[t->from[k]], *b = &dst->fields[t->to[k]];
    whole = t->from[k] == k && t->to[k] == k && a->type == b->type && a->len == b->len && a->dec == b->dec &&
            a->type != 'M';
  }
  t->whole = whole;
  for (uint32_t k = 0; k < t->count; ++k) {
    const xb_field_info *a = &src->fields[t->from[k]], *b = &dst->fields[t->to[k]];
    int numbers = strchr("NF", a->type) && strchr("NF", b->type);
    t->pair[k] = a->type == 'M' || b->type == 'M' ? PAIR_VALUE
                 : a->type == b->type && a->len == b->len && a->dec == b->dec ? PAIR_SAME
                 : numbers && t->text ? PAIR_NUMBERS
                 : PAIR_VALUE;
  }
  xb_area_pin(src);
  xb_area_pin(dst);
  src->snapshot = 1;
}

static void transfer_end(transfer *t) {
  t->source->snapshot = 0;
  xb_area_unpin(t->target);
  xb_area_unpin(t->source);
  free(t->from);
  free(t->to);
  free(t->later);
  free(t->bytes);
  free(t->pair);
}

/* __dbTrans( nTarget, aFields, [bFor], [bWhile], [nNext], [nRecord], [lRest] ): appends to the table of area nTarget
 * a copy of each record of the current area's scope (as dbEval's), the fields aFields names that both have; a deleted
 * record's copy is deleted. */
void xbf___DBTRANS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  xb_area *src = xb_db_record(ctx, "__DBTRANS");
  int64_t n = xb_db_num_arg(argc, argv, 0, 0);
  xb_area *dst = n >= 1 && n <= XB_AREA_MAX ? xb_area_at(ctx, (uint32_t)n) : NULL;
  const xb_array *list = argc > 1 && argv[1].type == XB_T_ARRAY ? argv[1].v.p : NULL;
  if (!src) return;
  if (!dst || dst == src || !list) {
    xb_error_of(ctx, XB_DBCMD, EG_ARG, 1015, "__DBTRANS", NULL, 0, 0, argc, argv, 0, NULL);
    return;
  }
  if (!xb_area_writable(ctx, dst)) return;
  transfer t;
  transfer_begin(&t, src, dst, list);
  if (t.count) xb_db_scope(ctx, src, argc, argv, 2, copy_record, &t);
  transfer_end(&t);
}

/* A new file of the fields given (new_file's), into which `run` copies the current area's records: closed after it.
 * 0 when it could not be made (the error raised). */
static int copy_into_new(xb_ctx *ctx, xb_area *src, const xb_str *name, const xb_rdd *rdd, xb_field_info *fields,
                         uint32_t count, void (*run)(xb_ctx *, transfer *, void *), void *arg) {
  uint32_t n = open_other(ctx, name, rdd, 1, fields, count, NULL);
  if (!n) return 0;
  transfer t;
  transfer_begin(&t, src, xb_area_at(ctx, n), NULL);
  run(ctx, &t, arg);
  xb_area *dst = t.target;
  transfer_end(&t);
  if (xb_area_at(ctx, n) == dst) xb_area_close(ctx, n); /* a block may have put another table there */
  return 1;
}

/* SORT's records: each one's number and its key, the keys' bytes side by side in an order memcmp() follows (`width`
 * bytes a record, in `keys`). */
typedef struct sort_key {
  uint32_t field; /* 0-based */
  int descend, nocase;
} sort_key;

typedef struct sorting {
  transfer *t;
  sort_key *key;        /* the keys, checked once */
  uint32_t nkeys;
  uint32_t argc;        /* the scope's arguments */
  xb_item *argv;
  size_t width, count, cap;
  uint32_t *recnos;
  unsigned char *keys;
} sorting;

/* A key's bytes for a field: a character field's text (ASCII letters upper-cased for `nocase`), a date's digits (the
 * empty date's blanks first), a logical 0 or 1, a number's value as 8 bytes that order as the numbers do (an integer
 * field of up to 18 digits as an integer, others as a double); all of them inverted for a descending key. */
static void put_key(const xb_field_info *fi, const unsigned char *p, int descend, int nocase, unsigned char *out) {
  uint32_t n = fi->len;
  switch (fi->type) {
  case 'C':
    for (uint32_t i = 0; i < n; ++i) out[i] = nocase ? (unsigned char)xb_ascii_upper((char)p[i]) : p[i];
    break;
  case 'D':
    memcpy(out, p, n);
    break;
  case 'L':
    n = 1;
    out[0] = xb_field_true((char)p[0]) ? 1 : 0;
    break;
  default: {
    xb_item v;
    xb_num_parse((const char *)p, fi->len, &v);
    uint64_t bits;
    if (fi->dec == 0 && fi->len < 19) { /* every value of the field fits an integer */
      int64_t i = v.type == XB_T_INT ? v.v.i : (int64_t)v.v.d;
      bits = (uint64_t)i ^ (UINT64_C(1) << 63);
    } else {
      double d = v.type == XB_T_INT ? (double)v.v.i : v.v.d;
      if (d == 0) d = 0; /* -0 orders as 0 */
      memcpy(&bits, &d, 8);
      bits = bits >> 63 ? ~bits : bits | (UINT64_C(1) << 63);
    }
    n = 8;
    for (int i = 0; i < 8; ++i) out[i] = (unsigned char)(bits >> (56 - 8 * i));
  }
  }
  if (descend)
    for (uint32_t i = 0; i < n; ++i) out[i] = (unsigned char)~out[i];
}

/* The bytes a field's key takes. */
static uint32_t key_width(const xb_field_info *fi) {
  return fi->type == 'L' ? 1 : fi->type == 'C' || fi->type == 'D' ? fi->len : 8;
}

static int take_key(xb_ctx *ctx, xb_area *src, void *arg) {
  sorting *s = arg;
  if (src->recno > s->t->last) return 1;
  if (!xb_area_load(ctx, src) || src->closed) return -1;
  if (s->count == s->cap) {
    size_t cap = s->cap ? s->cap * 2 : 1024;
    if (cap > UINT32_MAX || cap > SIZE_MAX / s->width || cap > SIZE_MAX / sizeof *s->recnos)
      xb_gc_fatal("out of memory");
    s->recnos = realloc(s->recnos, cap * sizeof *s->recnos);
    s->keys = realloc(s->keys, cap * s->width);
    if (!s->recnos || !s->keys) xb_gc_fatal("out of memory");
    s->cap = cap;
  }
  unsigned char *out = s->keys + s->count * s->width;
  for (uint32_t k = 0; k < s->nkeys; ++k) {
    const xb_field_info *fi = &src->fields[s->key[k].field];
    put_key(fi, src->rdd->field_bytes(src, s->key[k].field), s->key[k].descend, s->key[k].nocase, out);
    out += key_width(fi);
  }
  s->recnos[s->count++] = src->recno;
  return 1;
}

/* Sorts the positions `order` of the records by their keys, records of equal keys in the order they were taken. */
static void merge_sort(const sorting *s, uint32_t *order, uint32_t *tmp, size_t n) {
  for (size_t run = 1; run < n; run *= 2) {
    for (size_t lo = 0; lo < n; lo += 2 * run) {
      size_t mid = n - lo > run ? lo + run : n, hi = n - mid > run ? mid + run : n, a = lo, b = mid, o = lo;
      while (a < mid && b < hi) {
        int c = memcmp(s->keys + order[b] * s->width, s->keys + order[a] * s->width, s->width);
        tmp[o++] = c < 0 ? order[b++] : order[a++];
      }
      while (a < mid) tmp[o++] = order[a++];
      while (b < hi) tmp[o++] = order[b++];
    }
    memcpy(order, tmp, n * sizeof *order);
  }
}

/* SORT into the new file: the records of the scope taken with their keys, ordered, copied. */
static void sort_run(xb_ctx *ctx, transfer *t, void *arg) {
  sorting *s = arg;
  xb_area *src = t->source;
  s->t = t;
  t->deletedToo = 0;
  if (!s->nkeys) {
    xb_db_scope(ctx, src, s->argc, s->argv, 0, copy_record, t);
    return;
  }
  if (!xb_db_scope(ctx, src, s->argc, s->argv, 0, take_key, s) || src->closed || !s->count) return;
  uint32_t *order = malloc(s->count * sizeof *order), *tmp = malloc(s->count * sizeof *tmp);
  if (!order || !tmp) xb_gc_fatal("out of memory");
  for (size_t i = 0; i < s->count; ++i) order[i] = (uint32_t)i;
  merge_sort(s, order, tmp, s->count);
  for (size_t i = 0; i < s->count && !src->closed && !t->target->closed; ++i)
    if (!xb_area_goto(ctx, src, s->recnos[order[i]]) || copy_record(ctx, src, t) < 0) break;
  free(order);
  free(tmp);
}

/* __dbSortTo( cFile, [cDriver], aKeys, [bFor], [bWhile], [nNext], [nRecord], [lRest] ): a new file of the current
 * area's fields holding its records of the scope ordered by the keys aKeys gives ({ nField, lDescending, lNoCase }
 * each; records of equal keys in the order of the scope; without keys, the order of the scope), none deleted; the
 * area is left on the record copied last (without keys, where the scope ends). .F. when the file could not be made. */
void xbf___DBSORTTO(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_logical(ret, 0);
  xb_area *src = xb_db_record(ctx, "SORT");
  const xb_str *name = xb_db_str_arg(argc, argv, 0);
  const xb_array *spec = argc > 2 && argv[2].type == XB_T_ARRAY ? argv[2].v.p : NULL;
  if (!src) return;
  sorting s = {NULL, NULL, 0, argc > 3 ? argc - 3 : 0, argv + 3, 0, 0, 0, NULL, NULL};
  int ok = name && spec;
  s.key = malloc((ok && spec->len ? spec->len : 1) * sizeof *s.key);
  if (!s.key) xb_gc_fatal("out of memory");
  for (size_t k = 0; ok && k < spec->len; ++k) { /* checked and copied once: the blocks may change the array */
    const xb_array *row = spec->items[k].type == XB_T_ARRAY ? spec->items[k].v.p : NULL;
    int64_t f = row && row->len >= 3 && xb_is_number(&row->items[0]) ? xb_num_to_i64(&row->items[0]) : 0;
    ok = f >= 1 && f <= src->nfields && src->fields[f - 1].type != 'M' && row->items[1].type == XB_T_LOGICAL &&
         row->items[2].type == XB_T_LOGICAL && s.width + key_width(&src->fields[f - 1]) <= UINT32_MAX;
    if (!ok) break;
    s.key[s.nkeys++] = (sort_key){(uint32_t)(f - 1), row->items[1].v.l, row->items[2].v.l};
    s.width += key_width(&src->fields[f - 1]);
  }
  const xb_rdd *rdd;
  xb_field_info *fields;
  if (!ok) xb_error_of(ctx, XB_DBCMD, EG_ARG, 1015, "SORT", NULL, 0, 0, argc, argv, 0, NULL);
  else if (new_file(ctx, "SORT", argc, argv, 1, src->fields, src->nfields, &rdd, &fields)) {
    xb_set_logical(ret, copy_into_new(ctx, src, name, rdd, fields, src->nfields, sort_run, &s));
    free(fields);
  }
  free(s.key);
  free(s.recnos);
  free(s.keys);
}

/* TOTAL's groups: the key of the group the records belong to, whether its record is written, the sums. */
typedef struct totaling {
  transfer *t;
  xb_item *scope;                     /* the scope's arguments but FOR: NIL, WHILE, NEXT, RECORD, REST */
  const xb_item *keyBlock, *forBlock; /* NIL: one group; every record */
  xb_item key;                        /* a root */
  int open, written;
  uint32_t count;                     /* the fields summed */
  uint32_t *from, *to;
  double *sums;
} totaling;

/* Writes the group's sums into its record: a sum too wide for its field is "Data width error" (stars kept). 0 after
 * an error not defaulted. */
static int close_group(xb_ctx *ctx, totaling *g) {
  xb_area *dst = g->t->target;
  int ok = 1;
  for (uint32_t k = 0; ok && g->written && !dst->closed && k < g->count; ++k) {
    xb_item v;
    xb_set_double(&v, g->sums[k], (uint16_t)dst->fields[g->to[k]].dec);
    ok = (xb_area_field_put(ctx, dst, g->to[k], &v) || !(xb_pending_get(ctx) && xb_pending(ctx))) && !dst->closed;
  }
  g->open = g->written = 0;
  return ok;
}

static int total_record(xb_ctx *ctx, xb_area *src, void *arg) {
  totaling *g = arg;
  if (src->recno > g->t->last) return 1;
  xb_item k, f;
  xb_set_nil(&k);
  xb_set_nil(&f);
  xb_gc_root_push(ctx, &k);
  xb_gc_root_push(ctx, &f);
  int ok = g->keyBlock->type != XB_T_BLOCK || (xb_area_eval(ctx, src, g->keyBlock, &k) && !src->closed);
  if (ok && g->open) { /* the same group while the key is exactly the same */
    xb_item same;
    xb_op_exacteq(ctx, &same, &k, &g->key);
    ok = !(xb_pending_get(ctx) && xb_pending(ctx));
    if (ok && !(same.type == XB_T_LOGICAL && same.v.l)) ok = close_group(ctx, g);
  }
  if (ok && !g->open) {
    g->key = k;
    g->open = 1;
    for (uint32_t i = 0; i < g->count; ++i) g->sums[i] = 0;
  }
  int take = ok && (g->forBlock->type != XB_T_BLOCK ||
                    (xb_area_eval(ctx, src, g->forBlock, &f) && !src->closed && f.type == XB_T_LOGICAL && f.v.l));
  if (take && g->t->target->closed) ok = take = 0; /* a block closed the new file */
  if (take && !g->written) {
    ok = copy_record(ctx, src, g->t) > 0;
    g->written = ok;
  }
  if (ok && take) ok = xb_area_load(ctx, src) && !src->closed;
  for (uint32_t i = 0; ok && take && i < g->count; ++i) {
    xb_item v;
    xb_field_decode(&src->fields[g->from[i]], (const char *)src->rdd->field_bytes(src, g->from[i]), &v);
    g->sums[i] += v.type == XB_T_INT ? (double)v.v.i : v.v.d;
  }
  xb_gc_root_pop(ctx, 2);
  return ok && !(xb_pending_get(ctx) && xb_pending(ctx)) && !src->closed ? 1 : -1;
}

/* TOTAL into the new file: the scope's records in runs, a record for each. */
static void total_run(xb_ctx *ctx, transfer *t, void *arg) {
  totaling *g = arg;
  g->t = t;
  for (uint32_t k = 0; k < g->count; ++k) { /* each sum's field in the new file */
    const char *name = t->source->fields[g->from[k]].name;
    g->to[k] = (uint32_t)xb_area_field_index(t->target, name, strlen(name));
  }
  xb_gc_root_push(ctx, &g->key);
  if (xb_db_scope(ctx, t->source, 5, g->scope, 0, total_record, g) && !t->target->closed) close_group(ctx, g);
  xb_gc_root_pop(ctx, 1);
}

/* __dbTotalTo( cFile, [cDriver], [bKey], aSums, [bFor], [bWhile], [nNext], [nRecord], [lRest] ): a new file of the
 * current area's fields but its memos holding a record for each run of records of the scope whose key (bKey's value,
 * exactly the same; without bKey one run) is the same: the copy of its first record the FOR block takes (a deleted
 * one deleted), with the fields aSums names summed over the records it takes (a run none of them is taken from writes
 * nothing). A name of no number field is DBCMD/1015, raised before the file is made. .F. when it was not made. */
void xbf___DBTOTALTO(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_logical(ret, 0);
  xb_area *src = xb_db_record(ctx, "TOTAL");
  const xb_str *name = xb_db_str_arg(argc, argv, 0);
  const xb_array *sums = argc > 3 && argv[3].type == XB_T_ARRAY ? argv[3].v.p : NULL;
  if (!src) return;
  xb_item nil;
  xb_set_nil(&nil);
  xb_item scope[5] = {nil, argc > 5 ? argv[5] : nil, argc > 6 ? argv[6] : nil, argc > 7 ? argv[7] : nil,
                      argc > 8 ? argv[8] : nil};
  size_t cap = sums && sums->len ? sums->len : 1;
  totaling g = {NULL, scope, argc > 2 ? &argv[2] : &nil, argc > 4 ? &argv[4] : &nil, nil, 0, 0, 0, NULL, NULL, NULL};
  g.from = malloc(cap * sizeof *g.from);
  g.to = malloc(cap * sizeof *g.to);
  g.sums = malloc(cap * sizeof *g.sums);
  xb_field_info *kept = malloc((src->nfields ? src->nfields : 1) * sizeof *kept);
  if (!g.from || !g.to || !g.sums || !kept) xb_gc_fatal("out of memory");
  uint32_t count = 0;
  for (uint32_t k = 0; k < src->nfields; ++k)
    if (src->fields[k].type != 'M') kept[count++] = src->fields[k];
  int ok = name && sums;
  for (size_t k = 0; ok && k < sums->len; ++k) {
    const xb_item *it = &sums->items[k];
    int64_t from = it->type == XB_T_STRING ? xb_area_field_index(src, it->v.s->data, it->v.s->len) : -1;
    ok = from >= 0 && strchr("NF", src->fields[from].type);
    if (!ok) {
      xb_error_of(ctx, XB_DBCMD, EG_ARG, 1015, "TOTAL", NULL, 0, 0, 1, (xb_item *)it, 0, NULL);
      break;
    }
    g.from[g.count++] = (uint32_t)from;
  }
  if (!name || !sums) xb_error_of(ctx, XB_DBCMD, EG_ARG, 1015, "TOTAL", NULL, 0, 0, argc, argv, 0, NULL);
  const xb_rdd *rdd;
  xb_field_info *fields;
  if (ok && count && new_file(ctx, "TOTAL", argc, argv, 1, kept, count, &rdd, &fields)) {
    xb_set_logical(ret, copy_into_new(ctx, src, name, rdd, fields, count, total_run, &g));
    free(fields);
  }
  free(kept);
  free(g.from);
  free(g.to);
  free(g.sums);
}
