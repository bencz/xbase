/* The database functions of the language over the work areas (rdd/rdd.h). On an area with no table open, the
 * functions that only tell something give their neutral value (RecNo() 0, Eof() .T., FieldGet() NIL, ...); those that
 * move or change are "Workarea not in use" (DBCMD/2001). */
#include "rdd/rdd.h"

#include "core/file.h"
#include "rdd/order.h"

#include <stdlib.h>
#include <string.h>

const xb_str *xb_db_str_arg(uint32_t argc, const xb_item *argv, uint32_t k) {
  return argc > k && argv[k].type == XB_T_STRING ? argv[k].v.s : NULL;
}

int64_t xb_db_num_arg(uint32_t argc, const xb_item *argv, uint32_t k, int64_t dflt) {
  return argc > k && xb_is_number(&argv[k]) ? xb_num_to_i64(&argv[k]) : dflt;
}

int xb_db_logical_arg(uint32_t argc, const xb_item *argv, uint32_t k) {
  return argc > k && argv[k].type == XB_T_LOGICAL && argv[k].v.l;
}

xb_area *xb_db_used(xb_ctx *ctx, const char *fn) {
  xb_area *a = xb_area_current(ctx);
  if (!a) xb_area_unused_error(ctx, fn);
  return a;
}

/* The alias a table gets by default: its file name without directories and extension. */
static void default_alias(const xb_str *name, const char **p, size_t *len) {
  size_t start, ext;
  xb_file_name_parts(name->data, name->len, &start, &ext);
  *p = name->data + start;
  *len = ext - start;
}

static const xb_rdd *driver_arg(xb_ctx *ctx, uint32_t argc, const xb_item *argv, uint32_t k) {
  const xb_str *name = xb_db_str_arg(argc, argv, k);
  if (!name || name->len == 0) return xb_rdd_default(ctx);
  return xb_rdd_find(name->data, name->len);
}

/* Opens table `name` (".dbf" added when it has no extension) in the lowest free area (`newArea`, which becomes current
 * even when the table cannot be opened) or the current one, under `alias` (NULL or empty: the file name without
 * directories and extension). Returns 1, or 0 after the error (raised). */
static int open_table(xb_ctx *ctx, int newArea, const xb_rdd *rdd, const xb_str *name, const xb_str *alias,
                       int readonly, int exclusive) {
  if (newArea) ctx->areaCurrent = xb_area_free(ctx);
  const char *aliasText;
  size_t aliasLen;
  if (alias && alias->len) {
    aliasText = alias->data;
    aliasLen = alias->len;
  } else {
    default_alias(name, &aliasText, &aliasLen);
  }
  char *path = xb_file_path_with_ext(name->data, name->len, ".dbf");
  int ok = xb_area_open(ctx, ctx->areaCurrent, rdd, path, aliasText, aliasLen, readonly, exclusive);
  free(path);
  return ok;
}

/* dbUseArea( [lNewArea], [cDriver], cName, [cAlias], [lShared], [lReadOnly] ): lShared defaults to SET EXCLUSIVE
 * OFF. */
void xbf_DBUSEAREA(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  const xb_str *name = xb_db_str_arg(argc, argv, 2);
  const xb_rdd *rdd = driver_arg(ctx, argc, argv, 1);
  ctx->netErr = 0;
  if (!name || !rdd) {
    if (xb_db_logical_arg(argc, argv, 0)) ctx->areaCurrent = xb_area_free(ctx);
    xb_fn_arg_error(ctx, NULL, 1005, "DBUSEAREA", argc, argv);
    return;
  }
  int shared = argc > 4 && argv[4].type == XB_T_LOGICAL ? argv[4].v.l != 0 : ctx->set_shared;
  open_table(ctx, xb_db_logical_arg(argc, argv, 0), rdd, name, xb_db_str_arg(argc, argv, 3),
             xb_db_logical_arg(argc, argv, 5), !shared);
}

void xbf_DBCLOSEAREA(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area_close(ctx, ctx->areaCurrent);
}

void xbf_DBCLOSEALL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area_close_all(ctx);
  ctx->areaCurrent = 1;
}

/* dbSelectArea( nArea | cArea ): a number, or a string as xb_area_named reads it; 0 is the lowest free area. A
 * string that names no area is BASE/1002, which may be tried again. */
void xbf_DBSELECTAREA(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  const xb_str *alias = xb_db_str_arg(argc, argv, 0);
  int64_t n = xb_db_num_arg(argc, argv, 0, -1);
  for (uint16_t tries = 1; alias && (n = xb_area_named(ctx, alias->data, alias->len)) < 0; ++tries)
    if (xb_error_of(ctx, NULL, EG_NOALIAS, 1002, alias->data, NULL, 0, XB_ERR_RETRY, 0, NULL, tries, NULL) != 2)
      return;
  if (n == 0) ctx->areaCurrent = xb_area_free(ctx);
  else if (xb_area_number(n)) ctx->areaCurrent = xb_area_number(n);
}

/* Select( [cAlias] ): the current area's number, or the one a name gives as xb_area_named reads it (0 for none). */
void xbf_SELECT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_str *alias = xb_db_str_arg(argc, argv, 0);
  int64_t n = alias ? xb_area_named(ctx, alias->data, alias->len) : ctx->areaCurrent;
  xb_set_int(ret, n > 0 ? n : 0);
}

/* Alias( [nArea] ): the alias of the area (the current one), "" when none is open there. */
void xbf_ALIAS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  int64_t n = xb_db_num_arg(argc, argv, 0, 0);
  const xb_area *a = n > 0 && n <= UINT32_MAX ? xb_area_at(ctx, (uint32_t)n) : xb_area_current(ctx);
  xb_set_str(ret, a ? xb_str_new(a->alias, strlen(a->alias)) : xb_str_empty());
}

void xbf_USED(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_logical(ret, xb_area_current(ctx) != NULL);
}

void xbf_DBGOTO(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  xb_area *a = xb_db_used(ctx, "DBGOTO");
  int64_t n = xb_db_num_arg(argc, argv, 0, 0);
  if (a) xb_area_goto(ctx, a, n < 1 || n > UINT32_MAX ? 0 : (uint32_t)n);
}

void xbf_DBGOTOP(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_db_used(ctx, "DBGOTOP");
  if (a) xb_area_top(ctx, a);
}

void xbf_DBGOBOTTOM(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_db_used(ctx, "DBGOBOTTOM");
  if (a) xb_area_bottom(ctx, a);
}

void xbf_DBSKIP(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  xb_area *a = xb_db_used(ctx, "DBSKIP");
  if (a) xb_area_skip(ctx, a, xb_db_num_arg(argc, argv, 0, 1));
}

void xbf_BOF(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_area *a = xb_area_current(ctx);
  xb_set_logical(ret, !a || a->bof);
}

void xbf_EOF(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_area *a = xb_area_current(ctx);
  xb_set_logical(ret, !a || a->eof);
}

void xbf_FOUND(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_area *a = xb_area_current(ctx);
  xb_set_logical(ret, a && a->found);
}

void xbf_RECNO(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_area *a = xb_area_current(ctx);
  xb_set_int(ret, a ? a->recno : 0);
}

static void lastrec(xb_ctx *ctx, xb_item *ret) {
  xb_area *a = xb_area_current(ctx);
  xb_set_int(ret, a ? a->rdd->lastrec(a) : 0);
}
void xbf_LASTREC(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  lastrec(ctx, ret);
}
void xbf_RECCOUNT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  lastrec(ctx, ret);
}

void xbf_FCOUNT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_area *a = xb_area_current(ctx);
  xb_set_int(ret, a ? a->nfields : 0);
}

/* The 0-based field of argument `k` (1-based in the language), or -1. */
static int64_t field_arg(const xb_area *a, uint32_t argc, const xb_item *argv, uint32_t k) {
  int64_t n = xb_db_num_arg(argc, argv, k, 0);
  return a && n >= 1 && n <= a->nfields ? n - 1 : -1;
}

void xbf_FIELDGET(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  int64_t f = field_arg(a, argc, argv, 0);
  if (f >= 0) xb_area_field_get(ctx, a, (uint32_t)f, ret);
}

/* FieldPut( nField, xValue ): the value written, NIL when nothing was (NIL itself writes nothing). */
void xbf_FIELDPUT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  int64_t f = field_arg(a, argc, argv, 0);
  if (f < 0 || argc < 2 || argv[1].type == XB_T_NIL || a->eof) return;
  if (xb_area_field_put(ctx, a, (uint32_t)f, &argv[1])) *ret = argv[1];
}

void xbf_FIELDPOS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_area *a = xb_area_current(ctx);
  const xb_str *name = xb_db_str_arg(argc, argv, 0);
  xb_set_int(ret, a && name ? xb_area_field_index(a, name->data, name->len) + 1 : 0);
}

void xbf_FIELDNAME(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_area *a = xb_area_current(ctx);
  int64_t f = field_arg(a, argc, argv, 0);
  xb_set_str(ret, f >= 0 ? xb_str_new(a->fields[f].name, strlen(a->fields[f].name)) : xb_str_empty());
}

/* dbStruct(): { cName, cType, nLength, nDecimals } for each field, {} with no table. */
void xbf_DBSTRUCT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_area *a = xb_area_current(ctx);
  uint32_t n = a ? a->nfields : 0;
  xb_array *list = xb_array_alloc(n);
  xb_set_array(ret, list);
  for (uint32_t k = 0; k < n; ++k) {
    xb_array *row = xb_array_alloc(4);
    xb_set_array(&list->items[k], row);
    xb_gc_write_item(list, k);
    const xb_field_info *fi = &a->fields[k];
    xb_set_str(&row->items[0], xb_str_new(fi->name, strlen(fi->name)));
    xb_gc_write_item(row, 0);
    xb_set_str(&row->items[1], xb_str_new(&fi->type, 1));
    xb_gc_write_item(row, 1);
    xb_set_int(&row->items[2], fi->len);
    xb_set_int(&row->items[3], fi->dec);
  }
}

/* dbCreate( cFile, aStruct, [cDriver], [lNewArea], [cAlias] ): creates the table; with lNewArea (.T. a new area,
 * .F. the current one) opens it; when it cannot, the area current before stays current. A structure that is not
 * one, or one the driver cannot make, is DBCMD/1014. */
void xbf_DBCREATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  const xb_str *name = xb_db_str_arg(argc, argv, 0);
  const xb_rdd *rdd = driver_arg(ctx, argc, argv, 2);
  const xb_array *spec = argc > 1 && argv[1].type == XB_T_ARRAY ? argv[1].v.p : NULL;
  size_t n = spec ? spec->len : 0;
  int ok = name && rdd && n > 0 && n == (uint32_t)n;
  xb_field_info *fields = calloc(n ? n : 1, sizeof *fields);
  if (!fields) xb_gc_fatal("out of memory");
  for (size_t k = 0; ok && k < n; ++k) {
    const xb_array *row = spec->items[k].type == XB_T_ARRAY ? spec->items[k].v.p : NULL;
    ok = row && row->len >= 3 && row->items[0].type == XB_T_STRING && row->items[1].type == XB_T_STRING &&
         row->items[1].v.s->len > 0 && xb_is_number(&row->items[2]);
    if (!ok) break;
    xb_field_info *fi = &fields[k];
    const xb_str *fname = row->items[0].v.s;
    for (size_t i = 0; i < fname->len && i < XB_FIELD_NAME_SIZE - 1; ++i) fi->name[i] = xb_ascii_upper(fname->data[i]);
    fi->type = xb_ascii_upper(row->items[1].v.s->data[0]);
    int64_t len = xb_num_to_i64(&row->items[2]);
    int64_t dec = row->len > 3 && xb_is_number(&row->items[3]) ? xb_num_to_i64(&row->items[3]) : 0;
    ok = fi->name[0] && len >= 0 && len <= UINT32_MAX && dec >= 0 && dec <= UINT32_MAX;
    fi->len = ok ? (uint32_t)len : 0;
    fi->dec = ok ? (uint32_t)dec : 0;
    for (size_t j = 0; ok && j < k; ++j) ok = strcmp(fields[j].name, fi->name) != 0; /* names are unique */
  }
  if (!ok || !rdd->check(rdd, fields, (uint32_t)n)) {
    free(fields);
    xb_error_of(ctx, XB_DBCMD, EG_ARG, 1014, "DBCREATE", NULL, 0, 0, argc, argv, 0, NULL);
    return;
  }
  char *path = xb_file_path_with_ext(name->data, name->len, ".dbf");
  for (uint16_t tries = 1;; ++tries) {
    int32_t err = 0;
    if (rdd->create(rdd, path, fields, (uint32_t)n, &err)) break;
    if (xb_error_of(ctx, rdd->name, EG_CREATE, 1004, "", path, err, XB_ERR_RETRY | XB_ERR_DEFAULT, 0, NULL, tries,
                    NULL) != 2) {
      free(path);
      free(fields);
      return;
    }
  }
  free(path);
  free(fields);
  uint32_t before = ctx->areaCurrent;
  if (argc > 3 && argv[3].type == XB_T_LOGICAL &&
      !open_table(ctx, argv[3].v.l, rdd, name, xb_db_str_arg(argc, argv, 4), 0, !ctx->set_shared))
    ctx->areaCurrent = before; /* unlike dbUseArea(), the area current before stays */
}

void xbf_DBAPPEND(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_db_used(ctx, "DBAPPEND");
  if (!a || !xb_area_writable(ctx, a)) return;
  ctx->netErr = 0;
  a->found = 0;
  xb_area_append(ctx, a, !(argc > 0 && argv[0].type == XB_T_LOGICAL && !argv[0].v.l));
}

static void mark_deleted(xb_ctx *ctx, const char *fn, int deleted) {
  xb_area *a = xb_db_used(ctx, fn);
  if (a && xb_area_record_writable(ctx, a) && !a->eof && xb_area_hot(ctx, a, 0) && !a->closed && xb_area_load(ctx, a) &&
      !a->closed)
    a->rdd->set_deleted(a, deleted);
}
void xbf_DBDELETE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  mark_deleted(ctx, "DBDELETE", 1);
}
void xbf_DBRECALL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  mark_deleted(ctx, "DBRECALL", 0);
}

void xbf_DELETED(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_area_current(ctx);
  xb_set_logical(ret, a && xb_area_load(ctx, a) && xb_area_current(ctx) == a && a->rdd->deleted(a));
}

/* __dbPack(), __dbZap(): the deleted records, or all of them, go; then the first record. */
static void shrink(xb_ctx *ctx, const char *fn, int all) {
  xb_area *a = xb_db_used(ctx, fn);
  if (!a || !xb_area_writable(ctx, a) || !xb_area_exclusive(ctx, a)) return;
  int32_t err = 0;
  xb_area_pin(a);
  int cold = xb_area_cold(ctx, a) && !a->closed;
  xb_area_unpin(a);
  if (!cold) return;
  if (!(all ? a->rdd->zap(a, &err) : a->rdd->pack(a, &err))) {
    a->recno = a->rdd->lastrec(a) + 1; /* where the driver is left */
    a->bof = a->eof = 1;
    xb_area_error(ctx, a, EG_WRITE, 1011, "", err);
    return;
  }
  xb_area_pin(a);
  xb_area_orders_rebuild(ctx, a, NULL, 0);
  if (!a->closed) xb_area_top(ctx, a);
  xb_area_unpin(a);
}
void xbf___DBPACK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  shrink(ctx, "__DBPACK", 0);
}
void xbf___DBZAP(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  shrink(ctx, "__DBZAP", 1);
}

static void commit(xb_ctx *ctx, xb_area *a) {
  int32_t err = 0;
  xb_area_pin(a);
  if (xb_area_cold(ctx, a) && !a->closed && xb_area_orders_flush(ctx, a) && !a->rdd->flush(a, 1, &err))
    xb_area_error(ctx, a, EG_WRITE, 1011, "", err);
  xb_area_unpin(a);
}
void xbf_DBCOMMIT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_db_used(ctx, "DBCOMMIT");
  if (a) commit(ctx, a);
}
void xbf_DBCOMMITALL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  for (uint32_t n = 1; n <= ctx->areaCap; ++n)
    if (xb_area_at(ctx, n)) commit(ctx, xb_area_at(ctx, n));
}

void xbf_HEADER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_area_current(ctx);
  xb_set_int(ret, a ? a->rdd->header_size(a) : 0);
}

void xbf_RECSIZE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_area_current(ctx);
  xb_set_int(ret, a ? a->rdd->record_size(a) : 0);
}

void xbf_LUPDATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_area_current(ctx);
  xb_set_date(ret, a ? a->rdd->last_update(a) : 0);
}

/* dbSetFilter( bCondition, [cText] ), dbClearFilter(), dbFilter(): the records the block rejects do not show. */
void xbf_DBSETFILTER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  xb_area *a = xb_db_used(ctx, "DBSETFILTER");
  if (!a) return;
  if (argc > 0 && argv[0].type == XB_T_BLOCK) a->filter = argv[0];
  else xb_set_nil(&a->filter);
  if (xb_db_str_arg(argc, argv, 1)) a->filter_text = argv[1];
  else xb_set_str(&a->filter_text, xb_str_empty());
}

void xbf_DBCLEARFILTER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_area_current(ctx);
  if (!a) return;
  xb_set_nil(&a->filter);
  xb_set_str(&a->filter_text, xb_str_empty());
}

void xbf_DBFILTER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_area *a = xb_area_current(ctx);
  if (a) *ret = a->filter_text;
  else xb_set_str(ret, xb_str_empty());
}

/* Evaluates `block` (no arguments): whether it gave .T.; -1 after an error. */
static int test(xb_ctx *ctx, const xb_item *block) {
  if (block->type != XB_T_BLOCK) return 1;
  xb_item r;
  xb_set_nil(&r);
  xb_gc_root_push(ctx, &r);
  xb_block_call(ctx, &r, block, 0, &r);
  xb_gc_root_pop(ctx, 1);
  if (ctx->pending && xb_pending(ctx)) return -1;
  return r.type == XB_T_LOGICAL && r.v.l;
}

/* The scope of a command (FOR, WHILE, NEXT, RECORD, REST: the arguments from `k`): `visit` for each record of it
 * that the FOR block takes, until it returns 0 (stop) or -1 (error). The scope starts at the top unless WHILE, NEXT,
 * RECORD or REST says otherwise; NEXT n ends on its last record (none for n below 1). It is 0 after an error, or when
 * a block closed the area (`a` pinned by the caller). */
static int scope_run(xb_ctx *ctx, xb_area *a, uint32_t argc, const xb_item *argv, uint32_t k,
                     int (*visit)(xb_ctx *, xb_area *, void *), void *arg) {
  const xb_item *forBlock = argc > k ? &argv[k] : NULL, *whileBlock = argc > k + 1 ? &argv[k + 1] : NULL;
  int hasWhile = whileBlock && whileBlock->type == XB_T_BLOCK;
  int hasNext = argc > k + 2 && xb_is_number(&argv[k + 2]);
  int64_t next = xb_db_num_arg(argc, argv, k + 2, 0), record = xb_db_num_arg(argc, argv, k + 3, 0);
  int rest = xb_db_logical_arg(argc, argv, k + 4);
  if (record > 0) {
    if (!xb_area_goto(ctx, a, record > UINT32_MAX ? 0 : (uint32_t)record)) return 0;
    if (a->eof) return 1;
    int t = forBlock ? test(ctx, forBlock) : 1;
    if (t < 0 || a->closed) return 0;
    return t ? visit(ctx, a, arg) >= 0 && !a->closed : 1;
  }
  if (!hasWhile && !hasNext && !rest && !xb_area_top(ctx, a)) return 0;
  for (int64_t done = 0; !a->eof && (!hasNext || done < next);) {
    if (hasWhile) {
      int w = test(ctx, whileBlock);
      if (w <= 0 || a->closed) return w == 0 && !a->closed;
    }
    int t = forBlock ? test(ctx, forBlock) : 1;
    if (t < 0 || a->closed) return 0;
    if (t) {
      int v = visit(ctx, a, arg);
      if (a->closed) return 0;
      if (v <= 0) return v == 0;
    }
    if (hasNext && ++done >= next) break;
    if (!xb_area_skip(ctx, a, 1)) return 0;
  }
  return 1;
}

static int stop_here(xb_ctx *ctx, xb_area *a, void *arg) {
  XB_UNUSED(ctx);
  a->found = 1;
  *(int *)arg = 1;
  return 0;
}

/* __dbLocate( [bFor], [bWhile], [nNext], [nRecord], [lRest] ): moves to the first record of the scope the FOR block
 * takes (Found() .T.), else to the end of the scope (Found() .F.); __dbContinue() goes on from the next record. */
void xbf___DBLOCATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  xb_area *a = xb_db_used(ctx, "__DBLOCATE");
  if (!a) return;
  if (argc > 0) a->locate = argv[0];
  int hit = 0;
  xb_area_pin(a);
  scope_run(ctx, a, argc, argv, 0, stop_here, &hit);
  a->found = hit;
  xb_area_unpin(a);
}

void xbf___DBCONTINUE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_db_used(ctx, "__DBCONTINUE");
  if (!a || a->locate.type != XB_T_BLOCK) return;
  xb_item scope[5] = {a->locate}; /* rooted: the block may LOCATE again */
  for (int k = 1; k < 5; ++k) xb_set_nil(&scope[k]);
  xb_set_logical(&scope[4], 1); /* the rest of the table */
  xb_gc_root_push(ctx, &scope[0]);
  int hit = 0;
  xb_area_pin(a);
  if (xb_area_skip(ctx, a, 1) && !a->closed) scope_run(ctx, a, 5, scope, 0, stop_here, &hit);
  a->found = hit;
  xb_area_unpin(a);
  xb_gc_root_pop(ctx, 1);
}

static int eval_block(xb_ctx *ctx, xb_area *a, void *arg) {
  XB_UNUSED(a);
  xb_item r;
  xb_set_nil(&r);
  xb_gc_root_push(ctx, &r);
  xb_block_call(ctx, &r, (const xb_item *)arg, 0, &r);
  xb_gc_root_pop(ctx, 1);
  return ctx->pending && xb_pending(ctx) ? -1 : 1;
}

/* dbEval( bBlock, [bFor], [bWhile], [nNext], [nRecord], [lRest] ): evaluates bBlock on each record of the scope. */
void xbf_DBEVAL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  xb_area *a = xb_db_used(ctx, "DBEVAL");
  if (!a || argc < 1 || argv[0].type != XB_T_BLOCK) return;
  xb_area_pin(a);
  scope_run(ctx, a, argc, argv, 1, eval_block, &argv[0]);
  xb_area_unpin(a);
}

/* RLock() (Lock()): the current record's lock, the area's other record locks released; dbRLock( [nRecord] ): a
 * record's lock with the others kept (none given: as RLock()); FLock(): the file lock. .T. when had (an exclusive
 * table: always). */
void xbf_RLOCK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_logical(ret, 0);
  xb_area *a = xb_db_used(ctx, "RLOCK");
  if (a) xb_set_logical(ret, xb_area_lock(ctx, a, a->recno, 0));
}

void xbf_LOCK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { xbf_RLOCK(ctx, ret, argc, argv); }

void xbf_DBRLOCK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_logical(ret, 0);
  xb_area *a = xb_db_used(ctx, "DBRLOCK");
  if (!a) return;
  int64_t n = xb_db_num_arg(argc, argv, 0, 0);
  int given = n > 0 && n <= UINT32_MAX;
  xb_set_logical(ret, xb_area_lock(ctx, a, given ? (uint32_t)n : a->recno, given)); /* none given: as RLock() */
}

void xbf_FLOCK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_logical(ret, 0);
  xb_area *a = xb_db_used(ctx, "FLOCK");
  if (a) xb_set_logical(ret, xb_area_lock(ctx, a, 0, 0));
}

/* dbRUnlock( [nRecord] ): releases a record's lock (none given: every record lock); dbUnlock() (UNLOCK): every lock
 * of the area; dbUnlockAll() (UNLOCK ALL): every lock of every area. A change of the current record is written
 * first. */
void xbf_DBRUNLOCK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  xb_area *a = xb_db_used(ctx, "DBRUNLOCK");
  if (!a) return;
  int64_t n = xb_db_num_arg(argc, argv, 0, 0);
  if (n > 0 && n <= UINT32_MAX) xb_area_unlock(ctx, a, (uint32_t)n);
  else if (n == 0) xb_area_unlock_records(ctx, a);
}

void xbf_DBUNLOCK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_db_used(ctx, "DBUNLOCK");
  if (a) xb_area_unlock(ctx, a, 0);
}

void xbf_DBUNLOCKALL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  for (uint32_t n = 1; n <= ctx->areaCap; ++n)
    if (xb_area_at(ctx, n)) xb_area_unlock(ctx, xb_area_at(ctx, n), 0);
}

/* dbRLockList(): the record locks the area holds, in the order they were taken (none for an exclusive table, or
 * under the file lock). */
void xbf_DBRLOCKLIST(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_area *a = xb_area_current(ctx);
  uint32_t n = a ? a->nlocks : 0;
  xb_array *list = xb_array_alloc(n);
  xb_set_array(ret, list);
  for (uint32_t k = 0; k < n; ++k) xb_set_int(&list->items[k], a->locks[k]);
}

/* NetErr( [lNew] ): whether the last USE or APPEND failed for another user's lock; lNew sets it. */
void xbf_NETERR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_logical(ret, ctx->netErr);
  if (argc > 0 && argv[0].type == XB_T_LOGICAL) ctx->netErr = argv[0].v.l != 0;
}

/* RddSetDefault( [cDriver] ): the default driver's name before the call; an unknown name changes nothing. */
void xbf_RDDSETDEFAULT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  const xb_rdd *now = xb_rdd_default(ctx);
  xb_set_str(ret, xb_str_new(now->name, strlen(now->name)));
  const xb_str *name = xb_db_str_arg(argc, argv, 0);
  const xb_rdd *chosen = name && name->len ? xb_rdd_find(name->data, name->len) : NULL;
  if (chosen) ctx->rddDefault = chosen;
}

void xbf_RDDNAME(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_area *a = xb_db_used(ctx, "RDDNAME");
  if (a) xb_set_str(ret, xb_str_new(a->rdd->name, strlen(a->rdd->name)));
}

/* DBFNTX(), DBFCDX(): the drivers' names as functions, which REQUEST links in as Clipper's programs do; they do
 * nothing (every driver is always there). */
void xbf_DBFNTX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
}

void xbf_DBFCDX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { xbf_DBFNTX(ctx, ret, argc, argv); }
