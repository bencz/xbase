/* The primitives of the copies between tables and text files (docs/rdd.md#copies-between-files), which the commands'
 * functions in rdd/transfer.prg use: __dbTransOpen opens the other file in an area of its own, __dbTrans copies the
 * records of a scope. */
#include "rdd/rdd.h"

#include "core/file.h"

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

/* __dbTransOpen( cFile, [cDriver], lCreate, aStruct, [xDelimiter] ): the number of a new area (the lowest free one;
 * the current area stays current) holding the file (the driver's extension added to a name without one), 0 after an
 * error (raised). Created (lCreate): a table, or a text file, of the structure, opened exclusive; else a table opened
 * shared and read-only, or a text file read with the structure. xDelimiter: DELIM's delimiter. A driver of no name is
 * DBCMD/1015. */
void xbf___DBTRANSOPEN(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_int(ret, 0);
  const xb_str *name = xb_db_str_arg(argc, argv, 0);
  const xb_rdd *rdd = xb_db_driver_arg(ctx, argc, argv, 1);
  int create = xb_db_logical_arg(argc, argv, 2);
  xb_field_info *fields = NULL;
  uint32_t count = 0;
  int known = rdd && !rdd->text && !create; /* a table keeps its structure */
  if (!name || !rdd || (!known && (!xb_db_struct(argc > 3 ? &argv[3] : NULL, &fields, &count) ||
                                   !rdd->check(rdd, fields, count)))) {
    free(fields);
    xb_error_of(ctx, XB_DBCMD, EG_ARG, rdd ? 1014 : 1015, "__DBTRANSOPEN", NULL, 0, 0, argc, argv, 0, NULL);
    return;
  }
  char *path = xb_file_path_with_ext(name->data, name->len, rdd->ext);
  uint32_t n = xb_area_free(ctx), before = ctx->areaCurrent;
  xb_text_open text = {fields, count, create, argc > 4 ? &argv[4] : NULL};
  int ok;
  if (rdd->text) ok = xb_area_open_with(ctx, n, rdd, path, !create, create, &text);
  else ok = (!create || xb_db_create(ctx, rdd, path, fields, count)) &&
            xb_area_open_with(ctx, n, rdd, path, !create, create, NULL);
  ctx->areaCurrent = before;
  free(path);
  free(fields);
  if (ok) xb_set_int(ret, n);
}

/* What a copy takes from each record: the pairs of fields (source, target) of the same name, and whether the
 * records' bytes are copied whole (the same fields in the same places, no memo: the bytes as they are, blanks and
 * stars kept). From a text file, a field the line leaves blank stays blank, a number too wide for it stays stars;
 * others are read as the table's own. */
/* How copy_fields takes a pair of fields: as values; of the same kind and size (bytes in the stored form copied); two
 * number fields of other sizes (a text file's stars kept). */
enum { PAIR_VALUE, PAIR_SAME, PAIR_NUMBERS };

typedef struct transfer {
  xb_area *target;
  uint32_t *from, *to, *later, count; /* later: copy_fields's fields written as values */
  unsigned char *pair;                /* each pair's PAIR_ */
  unsigned char *bytes;               /* copy_fields's record of the target */
  int whole, text;
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
    ok = ok && (xb_area_field_put(ctx, dst, t->to[k], &v) || !(ctx->pending && xb_pending(ctx))) && !dst->closed &&
         !src->closed; /* the error block may close either */
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
  if (!dst->closed && (!ok || (!src->closed && src->rdd->deleted(src)))) dst->rdd->set_deleted(dst, 1);
  return ok ? 1 : -1;
}

/* __dbTrans( nTarget, aFields, [bFor], [bWhile], [nNext], [nRecord], [lRest] ): appends to the table of area nTarget
 * a copy of each record of the current area's scope (as dbEval's), the fields aFields names (names, or rows of a
 * structure) that both have, by name; a deleted record's copy is deleted. */
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
  transfer t = {dst, NULL, NULL, NULL, 0, NULL, NULL, 0, src->rdd->text, src->rdd->lastrec(src)};
  size_t cap = list->len ? list->len : 1;
  t.from = malloc(cap * sizeof *t.from);
  t.to = malloc(cap * sizeof *t.to);
  t.later = malloc(cap * sizeof *t.later);
  t.bytes = malloc(dst->rdd->record_size(dst));
  t.pair = malloc(cap);
  if (!t.from || !t.to || !t.later || !t.bytes || !t.pair) xb_gc_fatal("out of memory");
  for (size_t k = 0; k < list->len; ++k) {
    const xb_item *it = &list->items[k];
    const xb_array *row = it->type == XB_T_ARRAY ? it->v.p : NULL;
    if (row && row->len > 0) it = &row->items[0];
    if (it->type != XB_T_STRING) continue;
    int64_t from = xb_area_field_index(src, it->v.s->data, it->v.s->len);
    int64_t to = xb_area_field_index(dst, it->v.s->data, it->v.s->len);
    if (from < 0 || to < 0) continue;
    t.from[t.count] = (uint32_t)from;
    t.to[t.count++] = (uint32_t)to;
  }
  int whole = !src->rdd->text && t.count == src->nfields && t.count == dst->nfields;
  for (uint32_t k = 0; whole && k < t.count; ++k) {
    const xb_field_info *a = &src->fields[t.from[k]], *b = &dst->fields[t.to[k]];
    whole = t.from[k] == k && t.to[k] == k && a->type == b->type && a->len == b->len && a->dec == b->dec &&
            a->type != 'M';
  }
  t.whole = whole;
  for (uint32_t k = 0; k < t.count; ++k) {
    const xb_field_info *a = &src->fields[t.from[k]], *b = &dst->fields[t.to[k]];
    int numbers = strchr("NF", a->type) && strchr("NF", b->type);
    t.pair[k] = a->type == 'M' || b->type == 'M' ? PAIR_VALUE
                : a->type == b->type && a->len == b->len && a->dec == b->dec ? PAIR_SAME
                : numbers && t.text ? PAIR_NUMBERS
                : PAIR_VALUE;
  }
  xb_area_pin(src);
  xb_area_pin(dst);
  src->snapshot = 1;
  if (t.count) xb_db_scope(ctx, src, argc, argv, 2, copy_record, &t);
  src->snapshot = 0;
  xb_area_unpin(dst);
  xb_area_unpin(src);
  free(t.from);
  free(t.to);
  free(t.later);
  free(t.bytes);
  free(t.pair);
}
