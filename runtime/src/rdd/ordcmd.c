/* The order (index) functions of the language over the work areas' orders (rdd/order.h). An order is named by its
 * position (from 1) or its name, in any letter case; a function with no order to tell about gives its empty value. */
#include "rdd/order.h"

#include "core/file.h"

#include <stdlib.h>
#include <string.h>

/* The order an argument names in area `a` (a number from 1, or a name; 0, NIL or absent: the controlling one): its
 * position from 1, 0 for none. */
static uint32_t order_arg(const xb_area *a, uint32_t argc, const xb_item *argv, uint32_t k) {
  if (!a || argc <= k || argv[k].type == XB_T_NIL) return a ? a->order : 0;
  if (xb_is_number(&argv[k])) {
    int64_t n = xb_num_to_i64(&argv[k]);
    if (n == 0) return a->order;
    return n >= 1 && n <= a->norders ? (uint32_t)n : 0;
  }
  if (argv[k].type != XB_T_STRING) return 0;
  return xb_area_order_named(a, argv[k].v.s->data, argv[k].v.s->len, NULL);
}

static xb_order *order_at(xb_area *a, uint32_t n) { return n ? a->orders[n - 1] : NULL; }

/* The order an argument names where 0 is none (the order to make controlling, the flags ordIsUnique() and
 * ordDescend() tell): as order_arg() otherwise. */
static uint32_t order_or_none(const xb_area *a, uint32_t argc, const xb_item *argv) {
  if (argc > 0 && xb_is_number(&argv[0]) && xb_num_to_i64(&argv[0]) == 0) return 0;
  return order_arg(a, argc, argv, 0);
}

static void set_text(xb_item *ret, const char *s) { xb_set_str(ret, xb_str_new(s, strlen(s))); }

/* A bag's path: its name with the driver's extension (".ntx") when it has none (malloc'ed). */
static char *bag_path(const xb_area *a, const xb_str *name) {
  return xb_file_path_with_ext(name->data, name->len, a->rdd->index->ext);
}

/* The area's first order of bag `path`, 0 for none. */
static uint32_t bag_open(const xb_area *a, const char *path) {
  for (uint32_t i = 0; i < a->norders; ++i)
    if (strcmp(a->orders[i]->bag->path, path) == 0) return i + 1;
  return 0;
}

/* Closes order number `n` (1-based) of the area. */
static int close_order(xb_ctx *ctx, xb_area *a, uint32_t n) {
  xb_order *o = a->orders[n - 1];
  memmove(&a->orders[n - 1], &a->orders[n], (a->norders - n) * sizeof *a->orders);
  --a->norders;
  if (a->order == n) a->order = 0;
  else if (a->order > n) --a->order;
  return xb_order_close(ctx, a, o);
}

/* Whether the area's driver has its order files' format here: "Operation not supported" otherwise. */
static int orders_known(xb_ctx *ctx, const xb_area *a, const char *fn) {
  if (a->rdd->index) return 1;
  xb_error_of(ctx, a->rdd->name, EG_UNSUPPORTED, 1000, fn, NULL, 0, 0, 0, NULL, 0, NULL);
  return 0;
}

/* Whether the area's orders may change now: not while one of its key or condition blocks runs ("Operation not
 * supported" for the function `fn` then). */
static int orders_free(xb_ctx *ctx, xb_area *a, const char *fn) {
  if (!a->keying) return 1;
  xb_error_of(ctx, XB_DBCMD, EG_UNSUPPORTED, 1050, fn, NULL, 0, 0, 0, NULL, 0, NULL);
  return 0;
}

static void clear_pending(xb_area *a) { xb_order_conditions_clear(&a->pending); }

/* ordCondSet( [cFor], [bFor], [lAll], [bWhile], [bEval], [nInterval], [nStart], [nNext], [nRecord], [lRest],
 * [lDescend], [lCompound], [lAdditive], [lCurrent] ): the conditions and settings of the next order made in the
 * current area (xb_order_conditions; lAll is the default, lCompound is NTX's one order per file). */
void xbf_ORDCONDSET(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_logical(ret, 0);
  xb_area *a = xb_db_used(ctx, "ORDCONDSET");
  if (!a || !orders_free(ctx, a, "ORDCONDSET")) return;
  clear_pending(a);
  const xb_str *text = xb_db_str_arg(argc, argv, 0);
  xb_order_conditions *c = &a->pending;
  if (text && text->len) c->condText = argv[0];
  if (argc > 1 && argv[1].type == XB_T_BLOCK) c->cond = argv[1];
  if (argc > 3 && argv[3].type == XB_T_BLOCK) c->whileBlock = argv[3];
  if (argc > 4 && argv[4].type == XB_T_BLOCK) c->evalBlock = argv[4];
  c->every = xb_db_num_arg(argc, argv, 5, 1);
  c->start = xb_db_num_arg(argc, argv, 6, 0);
  c->next = xb_db_num_arg(argc, argv, 7, 0);
  c->record = xb_db_num_arg(argc, argv, 8, 0);
  c->rest = xb_db_logical_arg(argc, argv, 9);
  c->descend = xb_db_logical_arg(argc, argv, 10);
  c->additive = xb_db_logical_arg(argc, argv, 12);
  c->current = xb_db_logical_arg(argc, argv, 13);
  xb_set_logical(ret, 1);
}

/* ordCreate( [cBag], [cOrder], cKey, [bKey], [lUnique] ): builds the order from the records and opens it as the
 * controlling order, the others closed (but the production file's and its own file's) unless ordCondSet() asked to
 * keep them; then the first record. lUnique defaults to SET UNIQUE. One order per file (NTX): the file is cBag, else
 * cOrder, made anew. A tagged format (CDX): tag cOrder of file cBag (no cBag: the production file, named as the
 * table), added or replacing the tag of the name; without cOrder, file cBag made anew with one tag named as the
 * file. A production file made here is noted in the table's header. */
static void create_order(xb_ctx *ctx, const char *fn, const xb_str *bag, const xb_str *tag, const xb_str *keyText,
                         const xb_item *keyBlock, const xb_item *unique) {
  xb_area *a = xb_db_used(ctx, fn);
  if (!a || !orders_known(ctx, a, fn) || !orders_free(ctx, a, fn)) return;
  int tagged = a->rdd->index->tagged, named = tag && tag->len;
  if (bag && !bag->len) bag = NULL;
  if ((!bag && !named) || !keyText || !keyText->len) {
    xb_error_of(ctx, XB_DBCMD, EG_ARG, 1006, fn, NULL, 0, 0, 0, NULL, 0, NULL);
    return;
  }
  char *path = !tagged ? bag_path(a, bag ? bag : tag) : bag ? bag_path(a, bag) : NULL;
  if (!path) {
    path = malloc(strlen(a->production) + 1);
    if (!path) xb_gc_fatal("out of memory");
    strcpy(path, a->production);
  }
  char *name = NULL;
  if (tagged && named) {
    name = malloc(tag->len + 1);
    if (!name) xb_gc_fatal("out of memory");
    memcpy(name, tag->data, tag->len);
    name[tag->len] = '\0';
  }
  int fresh = !name;
  xb_area_pin(a);
  int ok = xb_area_cold(ctx, a);
  uint32_t same = ok && !a->closed ? bag_open(a, path) : 0;
  int wasOpen = same != 0;
  if (same && fresh) { /* the file is made anew: its orders go */
    while (ok && !a->closed && (same = bag_open(a, path)) != 0) ok = close_order(ctx, a, same);
  } else if (same) { /* a tag of the name is replaced */
    uint32_t old = xb_area_order_named(a, name, strlen(name), a->orders[same - 1]->bag->path);
    if (old) ok = close_order(ctx, a, old);
  }
  xb_order *o = NULL;
  if (ok && !a->closed) { /* built while the other orders are open: USECURRENT, REST, ... walk the controlling one */
    int isUnique = unique && unique->type == XB_T_LOGICAL ? unique->v.l != 0 : ctx->set_unique;
    o = xb_order_create(ctx, a, path, name, fresh, keyText, keyBlock, isUnique);
  }
  if (o && !a->closed && !a->pending.additive) xb_area_orders_clear(ctx, a, o->bag);
  if (!a->closed) clear_pending(a);
  if (o && a->closed) {
    xb_order_close(ctx, a, o);
    o = NULL;
  }
  if (o) {
    a->order = xb_area_add_order(a, o);
    int32_t err = 0;
    if (o->bag->production && !wasOpen && !a->rdd->set_production(a, 1, &err))
      xb_area_error(ctx, a, EG_WRITE, 1011, "", err);
    if (!a->closed) xb_area_top(ctx, a);
  }
  xb_area_unpin(a);
  free(name);
  free(path);
}

void xbf_ORDCREATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  create_order(ctx, "ORDCREATE", xb_db_str_arg(argc, argv, 0), xb_db_str_arg(argc, argv, 1),
               xb_db_str_arg(argc, argv, 2), argc > 3 ? &argv[3] : NULL, argc > 4 ? &argv[4] : NULL);
}

/* dbCreateIndex( cFile, cKey, [bKey], [lUnique] ): ordCreate( cFile, , cKey, bKey, lUnique ). */
void xbf_DBCREATEINDEX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  create_order(ctx, "DBCREATEINDEX", xb_db_str_arg(argc, argv, 0), NULL, xb_db_str_arg(argc, argv, 1),
               argc > 2 ? &argv[2] : NULL, argc > 3 ? &argv[3] : NULL);
}

/* ordDestroy( xOrder, [cBag] ): the order (in file cBag when given) leaves its file and closes; a file left without
 * orders is erased. Nothing when there is no such order. */
void xbf_ORDDESTROY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  xb_area *a = xb_db_used(ctx, "ORDDESTROY");
  if (!a || !orders_known(ctx, a, "ORDDESTROY") || !orders_free(ctx, a, "ORDDESTROY") || argc == 0 ||
      argv[0].type == XB_T_NIL)
    return;
  if (!xb_area_writable(ctx, a)) return;
  const xb_str *bag = xb_db_str_arg(argc, argv, 1);
  char *path = bag && bag->len ? bag_path(a, bag) : NULL;
  xb_area_pin(a);
  uint32_t n = 0;
  if (xb_area_cold(ctx, a) && !a->closed) /* the order found once the change is written: its handler may close it */
    n = path && argv[0].type == XB_T_STRING ? xb_area_order_named(a, argv[0].v.s->data, argv[0].v.s->len, path)
                                             : order_arg(a, argc, argv, 0);
  free(path);
  if (n) {
    xb_order *o = a->orders[n - 1];
    memmove(&a->orders[n - 1], &a->orders[n], (a->norders - n) * sizeof *a->orders);
    --a->norders;
    if (a->order == n) a->order = 0;
    else if (a->order > n) --a->order;
    xb_order_destroy(ctx, a, o);
  }
  xb_area_unpin(a);
}

/* ordCount( [cBag] ): the orders open in the current area (in file cBag when given). */
void xbf_ORDCOUNT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  const xb_str *bag = xb_db_str_arg(argc, argv, 0);
  int64_t n = 0;
  if (a && a->rdd->index) {
    char *path = bag && bag->len ? bag_path(a, bag) : NULL;
    for (uint32_t i = 0; i < a->norders; ++i) n += !path || strcmp(a->orders[i]->bag->path, path) == 0;
    free(path);
  }
  xb_set_int(ret, n);
}

/* ordListAdd( cBag ), dbSetIndex( cBag ): opens the order of a file (not again when it is open); when no order was
 * controlling, it becomes the controlling one, and the first record current. */
static void add_bag(xb_ctx *ctx, const char *fn, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_db_used(ctx, fn);
  const xb_str *bag = xb_db_str_arg(argc, argv, 0);
  if (!a || !bag || !bag->len || !orders_known(ctx, a, fn) || !orders_free(ctx, a, fn)) return;
  char *path = bag_path(a, bag);
  xb_area_pin(a);
  if (!bag_open(a, path) && xb_area_cold(ctx, a) && !a->closed) {
    xb_order **list = NULL;
    uint32_t n = xb_order_open(ctx, a, path, &list), first = 0;
    for (uint32_t i = 0; i < n; ++i) {
      if (a->closed) xb_order_close(ctx, a, list[i]);
      else if (!first) first = xb_area_add_order(a, list[i]);
      else xb_area_add_order(a, list[i]);
    }
    free(list);
    if (n && !a->closed && !a->order) {
      a->order = first;
      xb_area_top(ctx, a);
    }
  }
  xb_area_unpin(a);
  free(path);
}

void xbf_ORDLISTADD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  add_bag(ctx, "ORDLISTADD", argc, argv);
}

void xbf_DBSETINDEX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  add_bag(ctx, "DBSETINDEX", argc, argv);
}

/* ordListClear(), dbClearIndex(): the orders closed but the production file's; the record order is controlling. */
static void clear_orders(xb_ctx *ctx, const char *fn) {
  xb_area *a = xb_area_current(ctx);
  if (!a || !orders_free(ctx, a, fn)) return;
  xb_area_pin(a);
  xb_area_orders_clear(ctx, a, NULL);
  xb_area_unpin(a);
}

void xbf_ORDLISTCLEAR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  clear_orders(ctx, "ORDLISTCLEAR");
}

void xbf_DBCLEARINDEX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  clear_orders(ctx, "DBCLEARINDEX");
}

/* ordListRebuild(), dbReindex(): every order's file made again from the records; then the first record. */
static void rebuild(xb_ctx *ctx, const char *fn) {
  xb_area *a = xb_db_used(ctx, fn);
  if (!a || !xb_area_writable(ctx, a) || !xb_area_exclusive(ctx, a) || !orders_free(ctx, a, fn)) return;
  xb_area_pin(a);
  if (xb_area_cold(ctx, a) && !a->closed) xb_area_orders_rebuild(ctx, a, &a->pending.evalBlock, a->pending.every);
  if (!a->closed) {
    clear_pending(a); /* ordCondSet()'s EVAL served the rebuild */
    xb_area_top(ctx, a);
  }
  xb_area_unpin(a);
}

void xbf_ORDLISTREBUILD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  rebuild(ctx, "ORDLISTREBUILD");
}

void xbf_DBREINDEX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  rebuild(ctx, "DBREINDEX");
}

/* ordSetFocus( [xOrder] ): the controlling order's name before the call ("" for none); xOrder (0 or one that is not
 * there: none) makes another controlling. The record stays. */
void xbf_ORDSETFOCUS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  xb_order *before = a ? xb_area_order(a) : NULL;
  set_text(ret, before ? before->name : "");
  if (a && argc > 0 && argv[0].type != XB_T_NIL) a->order = order_or_none(a, argc, argv);
}

/* dbSetOrder( nOrder ): as ordSetFocus() with a number. */
void xbf_DBSETORDER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_db_used(ctx, "DBSETORDER");
  if (!a) return;
  xb_order *before = xb_area_order(a);
  set_text(ret, before ? before->name : "");
  if (argc > 0 && xb_is_number(&argv[0])) a->order = order_or_none(a, argc, argv);
}

/* dbSeek( xKey, [lSoft], [lLast] ): SEEK in the controlling order (lSoft defaults to SET SOFTSEEK): Found(). */
void xbf_DBSEEK(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_db_used(ctx, "DBSEEK");
  xb_set_logical(ret, 0);
  if (!a) return;
  if (argc == 0) {
    xb_error_of(ctx, XB_DBCMD, EG_ARG, 1001, "DBSEEK", NULL, 0, 0, 0, NULL, 0, NULL);
    return;
  }
  int soft = argc > 1 && argv[1].type == XB_T_LOGICAL ? argv[1].v.l : ctx->set_softseek;
  xb_item nil;
  xb_set_nil(&nil);
  if (!xb_area_seek(ctx, a, argc > 0 ? &argv[0] : &nil, soft, xb_db_logical_arg(argc, argv, 2))) return;
  a = xb_area_current(ctx);
  xb_set_logical(ret, a && a->found);
}

void xbf_INDEXORD(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_area_current(ctx);
  xb_set_int(ret, a ? a->order : 0);
}

/* IndexKey( [nOrder] ), ordKey( [xOrder] ): the order's key expression ("" for none; 0 or none given: the
 * controlling order's). */
void xbf_INDEXKEY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  int64_t n = xb_db_num_arg(argc, argv, 0, 0);
  xb_order *o = a ? order_at(a, n == 0 ? a->order : n >= 1 && n <= a->norders ? (uint32_t)n : 0) : NULL;
  set_text(ret, o ? o->info.key : "");
}

void xbf_ORDKEY(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  xb_order *o = a ? order_at(a, order_arg(a, argc, argv, 0)) : NULL;
  set_text(ret, o ? o->info.key : "");
}

void xbf_ORDFOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  xb_order *o = a ? order_at(a, order_arg(a, argc, argv, 0)) : NULL;
  set_text(ret, o ? o->info.cond : "");
}

/* ordName( [nOrder] ): the order's name (upper case). */
void xbf_ORDNAME(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  xb_order *o = a ? order_at(a, order_arg(a, argc, argv, 0)) : NULL;
  set_text(ret, o ? o->name : "");
}

/* ordNumber( cName ): the order's position, 0 for none. */
void xbf_ORDNUMBER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  xb_set_int(ret, a && argc > 0 && argv[0].type == XB_T_STRING ? order_arg(a, argc, argv, 0) : 0);
}

/* ordBagName( [xOrder] ): the order's file name, as given, without directories and extension. */
void xbf_ORDBAGNAME(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  xb_order *o = a ? order_at(a, order_arg(a, argc, argv, 0)) : NULL;
  if (!o) {
    set_text(ret, "");
    return;
  }
  size_t start, ext;
  const char *path = o->bag->path;
  xb_file_name_parts(path, strlen(path), &start, &ext);
  xb_set_str(ret, xb_str_new(path + start, ext - start));
}

void xbf_ORDISUNIQUE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  xb_order *o = a ? order_at(a, order_or_none(a, argc, argv)) : NULL;
  xb_set_logical(ret, o && o->info.unique);
}

void xbf_ORDDESCEND(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_area_current(ctx);
  xb_order *o = a ? order_at(a, order_or_none(a, argc, argv)) : NULL;
  xb_set_logical(ret, o && o->info.descend);
}

/* ordBagExt(), IndexExt(): the extension of the order files of the current area's driver (none open: the default
 * driver's). */
void xbf_ORDBAGEXT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  const xb_area *a = xb_area_current(ctx);
  const xb_index_format *f = (a ? a->rdd : xb_rdd_default(ctx))->index;
  set_text(ret, f ? f->ext : "");
}

void xbf_INDEXEXT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) { xbf_ORDBAGEXT(ctx, ret, argc, argv); }

/* ordScope( nScope, [xValue] ): the controlling order's scope bound before (0 top, 1 bottom; NIL for none);
 * xValue sets it (a codeblock: evaluated again before each navigation), NIL clears it. nScope 2 sets both bounds
 * (NIL or no xValue: clears them) and gives NIL. The record stays where it is. */
void xbf_ORDSCOPE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_db_used(ctx, "ORDSCOPE");
  xb_order *o = a ? xb_area_order(a) : NULL;
  if (!o) return;
  int64_t which = xb_db_num_arg(argc, argv, 0, 0);
  xb_item nil;
  xb_set_nil(&nil);
  const xb_item *v = argc > 1 ? &argv[1] : &nil;
  xb_area_pin(a); /* a codeblock may close the area */
  if (which == 2) {
    if (xb_order_set_scope(ctx, a, o, 0, v)) xb_order_set_scope(ctx, a, o, 1, v);
  } else {
    int bound = which == 0 ? 0 : 1;
    *ret = o->scope[bound].value;
    if (argc > 1) xb_order_set_scope(ctx, a, o, bound, v);
  }
  xb_area_unpin(a);
}

/* ordKeyCount( [xOrder] ): the order's entries in its scope (the filter and SET DELETED do not count); with no
 * order, LastRec(). */
void xbf_ORDKEYCOUNT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_int(ret, 0);
  xb_area *a = xb_db_used(ctx, "ORDKEYCOUNT");
  if (!a) return;
  xb_order *o = order_at(a, order_arg(a, argc, argv, 0));
  uint64_t n = a->rdd->lastrec(a);
  xb_area_pin(a);
  int ok = !o || (xb_area_cold(ctx, a) && !a->closed && xb_order_count(ctx, a, o, &n));
  xb_area_unpin(a);
  if (ok) xb_set_int(ret, (int64_t)n);
}

/* ordKeyNo( [xOrder] ): the current record's position among the order's entries in its scope, 0 when it has none
 * there; with no order, RecNo(). */
void xbf_ORDKEYNO(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_int(ret, 0);
  xb_area *a = xb_db_used(ctx, "ORDKEYNO");
  if (!a) return;
  xb_order *o = order_at(a, order_arg(a, argc, argv, 0));
  uint64_t n = a->recno;
  xb_area_pin(a);
  int ok = !o || (xb_area_cold(ctx, a) && !a->closed && xb_order_position(ctx, a, o, &n));
  xb_area_unpin(a);
  if (ok) xb_set_int(ret, (int64_t)n);
}

/* ordKeyGoto( nKeyNo ): to the record of the controlling order's entry nKeyNo in its scope (past the last: the end),
 * then forward to the first that shows; .F. for a position below 1. With no order, GO nKeyNo. */
void xbf_ORDKEYGOTO(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_logical(ret, 0);
  xb_area *a = xb_db_used(ctx, "ORDKEYGOTO");
  if (!a || argc == 0 || !xb_is_number(&argv[0])) return;
  int64_t n = xb_num_to_i64(&argv[0]);
  xb_order *o = xb_area_order(a);
  if (!o) {
    xb_set_logical(ret, xb_area_goto(ctx, a, n > 0 && n <= UINT32_MAX ? (uint32_t)n : 0));
    return;
  }
  if (n < 1) return;
  xb_area_pin(a);
  uint32_t recno = 0;
  int ok = xb_area_cold(ctx, a) && !a->closed && xb_order_nth(ctx, a, o, (uint64_t)n, &recno) && !a->closed &&
           xb_area_arrive(ctx, a, recno, 1);
  xb_area_unpin(a);
  xb_set_logical(ret, ok);
}

/* ordKeyVal(): the current record's key in the controlling order (a number with the key's width and decimals), NIL
 * when it has no entry or there is no order. */
void xbf_ORDKEYVAL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_area *a = xb_db_used(ctx, "ORDKEYVAL");
  xb_order *o = a ? xb_area_order(a) : NULL;
  if (!o || a->eof) return;
  unsigned char key[XB_INDEX_KEY_MAX];
  int in = 0;
  xb_area_pin(a);
  int ok = xb_order_record_key(ctx, a, o, key, &in) && !a->closed;
  xb_area_unpin(a);
  if (ok && in) xb_order_key_value(o, key, xb_decimals(ctx), ret);
}

/* ordSkipUnique( [nDirection] ): to the next key's first record (nDirection below 0: the previous key's last) in the
 * controlling order (xb_order_skip_unique); with no order, SKIP nDirection. .T. but after an error. */
void xbf_ORDSKIPUNIQUE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_set_logical(ret, 0);
  xb_area *a = xb_db_used(ctx, "ORDSKIPUNIQUE");
  if (!a) return;
  int64_t n = argc > 0 && xb_is_number(&argv[0]) ? xb_num_to_i64(&argv[0]) : 1;
  xb_order *o = xb_area_order(a);
  xb_area_pin(a);
  int ok = o ? xb_area_cold(ctx, a) && !a->closed && xb_order_skip_unique(ctx, a, o, n < 0 ? -1 : 1)
             : xb_area_skip(ctx, a, n);
  xb_area_unpin(a);
  xb_set_logical(ret, ok);
}
