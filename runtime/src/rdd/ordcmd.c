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
  const xb_str *s = argv[k].v.s;
  size_t len = s->len;
  while (len > 0 && s->data[len - 1] == ' ') --len;
  if (len > XB_ORDER_NAME_MAX) len = XB_ORDER_NAME_MAX; /* names are kept that long */
  for (uint32_t i = 0; i < a->norders; ++i) {
    const char *name = a->orders[i]->name;
    size_t j = 0;
    while (j < len && name[j] && xb_ascii_upper(s->data[j]) == name[j]) ++j;
    if (j == len && !name[j]) return i + 1;
  }
  return 0;
}

static xb_order *order_at(xb_area *a, uint32_t n) { return n ? a->orders[n - 1] : NULL; }

/* The order an argument names where 0 is none (the order to make controlling, the flags ordIsUnique() and
 * ordDescend() tell): as order_arg() otherwise. */
static uint32_t order_or_none(const xb_area *a, uint32_t argc, const xb_item *argv) {
  if (argc > 0 && xb_is_number(&argv[0]) && xb_num_to_i64(&argv[0]) == 0) return 0;
  return order_arg(a, argc, argv, 0);
}

static void set_text(xb_item *ret, const char *s) { xb_set_str(ret, xb_str_new(s, strlen(s))); }

/* A bag's path: its name with ".ntx" when it has none (malloc'ed). */
static char *bag_path(const xb_str *name) { return xb_file_path_with_ext(name->data, name->len, XB_NTX_EXT); }

/* The area's order of bag `path`, 0 for none. */
static uint32_t bag_open(const xb_area *a, const char *path) {
  for (uint32_t i = 0; i < a->norders; ++i)
    if (strcmp(a->orders[i]->bag, path) == 0) return i + 1;
  return 0;
}

static void add_order(xb_area *a, xb_order *o) {
  xb_order **grown = realloc(a->orders, (a->norders + 1) * sizeof *grown);
  if (!grown) xb_gc_fatal("out of memory");
  a->orders = grown;
  a->orders[a->norders++] = o;
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

/* Whether the area's orders may change now: not while one of its key or condition blocks runs ("Operation not
 * supported" for the function `fn` then). */
static int orders_free(xb_ctx *ctx, xb_area *a, const char *fn) {
  if (!a->keying) return 1;
  xb_error_of(ctx, XB_DBCMD, EG_UNSUPPORTED, 1050, fn, NULL, 0, 0, 0, NULL, 0, NULL);
  return 0;
}

static void clear_pending(xb_area *a) {
  xb_set_nil(&a->pendingCond);
  xb_set_nil(&a->pendingCondText);
  a->pendingDescend = a->pendingAdditive = 0;
}

/* ordCondSet( [cFor], [bFor], [lAll], [bWhile], [bEval], [nInterval], [nStart], [nNext], [nRecord], [lRest],
 * [lDescend], [lCompound], [lAdditive] ): the condition and the settings of the next order made in the current area
 * (its FOR condition, descending, kept with the open orders). */
void xbf_ORDCONDSET(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_db_used(ctx, "ORDCONDSET");
  if (!a) return;
  clear_pending(a);
  const xb_str *text = xb_db_str_arg(argc, argv, 0);
  if (text && text->len) a->pendingCondText = argv[0];
  if (argc > 1 && argv[1].type == XB_T_BLOCK) a->pendingCond = argv[1];
  a->pendingDescend = xb_db_logical_arg(argc, argv, 10);
  a->pendingAdditive = xb_db_logical_arg(argc, argv, 12);
  xb_set_logical(ret, 1);
}

/* ordCreate( cBag, [cOrder], cKey, [bKey], [lUnique] ): builds the order's file from the records and opens it as the
 * controlling order, the others closed unless ordCondSet() asked to keep them; then the first record. lUnique
 * defaults to SET UNIQUE. */
static void create_order(xb_ctx *ctx, const char *fn, const xb_str *bag, const xb_str *keyText, const xb_item *keyBlock,
                         const xb_item *unique) {
  xb_area *a = xb_db_used(ctx, fn);
  if (!a || !orders_free(ctx, a, fn)) return;
  if (!bag || !bag->len || !keyText || !keyText->len) {
    xb_error_of(ctx, XB_DBCMD, EG_ARG, 1006, fn, NULL, 0, 0, 0, NULL, 0, NULL);
    return;
  }
  char *path = bag_path(bag);
  xb_area_pin(a);
  int ok = a->pendingAdditive ? xb_area_cold(ctx, a) : xb_area_orders_close(ctx, a);
  uint32_t same = ok && !a->closed ? bag_open(a, path) : 0;
  if (same) ok = close_order(ctx, a, same); /* the file is made again */
  xb_order *o = NULL;
  if (ok && !a->closed) {
    int isUnique = unique && unique->type == XB_T_LOGICAL ? unique->v.l != 0 : ctx->set_unique;
    o = xb_order_create(ctx, a, path, keyText, keyBlock, isUnique);
  }
  if (!a->closed) clear_pending(a);
  if (o && a->closed) {
    int32_t ignored;
    xb_ntx_close(o->ntx, &ignored);
    o->ntx = NULL;
    xb_order_close(ctx, a, o);
    o = NULL;
  }
  if (o) {
    add_order(a, o);
    a->order = a->norders;
    xb_area_top(ctx, a);
  }
  xb_area_unpin(a);
  free(path);
}

void xbf_ORDCREATE(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  const xb_str *bag = xb_db_str_arg(argc, argv, 0);
  if (!bag || !bag->len) bag = xb_db_str_arg(argc, argv, 1); /* no bag: the order's name is the file's */
  create_order(ctx, "ORDCREATE", bag, xb_db_str_arg(argc, argv, 2), argc > 3 ? &argv[3] : NULL,
               argc > 4 ? &argv[4] : NULL);
}

/* dbCreateIndex( cFile, cKey, [bKey], [lUnique] ): ordCreate( cFile, , cKey, bKey, lUnique ). */
void xbf_DBCREATEINDEX(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ret);
  create_order(ctx, "DBCREATEINDEX", xb_db_str_arg(argc, argv, 0), xb_db_str_arg(argc, argv, 1),
               argc > 2 ? &argv[2] : NULL, argc > 3 ? &argv[3] : NULL);
}

/* ordListAdd( cBag ), dbSetIndex( cBag ): opens the order of a file (not again when it is open); when no order was
 * controlling, it becomes the controlling one, and the first record current. */
static void add_bag(xb_ctx *ctx, const char *fn, uint32_t argc, xb_item *argv) {
  xb_area *a = xb_db_used(ctx, fn);
  const xb_str *bag = xb_db_str_arg(argc, argv, 0);
  if (!a || !bag || !bag->len || !orders_free(ctx, a, fn)) return;
  char *path = bag_path(bag);
  xb_area_pin(a);
  if (!bag_open(a, path) && xb_area_cold(ctx, a) && !a->closed) {
    xb_order *o = xb_order_open(ctx, a, path);
    if (o && a->closed) {
      xb_order_close(ctx, a, o);
    } else if (o) {
      add_order(a, o);
      if (!a->order) {
        a->order = a->norders;
        xb_area_top(ctx, a);
      }
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

/* ordListClear(), dbClearIndex(): the orders closed; the record order is controlling. */
static void clear_orders(xb_ctx *ctx, const char *fn) {
  xb_area *a = xb_area_current(ctx);
  if (!a || !orders_free(ctx, a, fn)) return;
  xb_area_pin(a);
  xb_area_orders_close(ctx, a);
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
  if (!a || !xb_area_writable(ctx, a) || !orders_free(ctx, a, fn)) return;
  xb_area_pin(a);
  if (xb_area_cold(ctx, a) && !a->closed) xb_area_orders_rebuild(ctx, a);
  if (!a->closed) xb_area_top(ctx, a);
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
  xb_file_name_parts(o->bag, strlen(o->bag), &start, &ext);
  xb_set_str(ret, xb_str_new(o->bag + start, ext - start));
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

/* ordBagExt(), IndexExt(): the extension of the default driver's order files. */
void xbf_ORDBAGEXT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  set_text(ret, XB_NTX_EXT);
}

void xbf_INDEXEXT(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  set_text(ret, XB_NTX_EXT);
}
