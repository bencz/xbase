/* Orders (indexes) of a work area (rdd/order.c): a record's key, the entries kept up to date as records change, and
 * the operations navigation and SEEK use. An order's file is an NTX one (rdd/ntx.c). */
#ifndef XB_ORDER_H
#define XB_ORDER_H

#include "rdd/ntx.h"
#include "rdd/rdd.h"

#define XB_ORDER_NAME_MAX 10 /* characters of an order's name, as NTX keeps them */

struct xb_order {
  char *bag;               /* the file's path (malloc'ed) */
  char *name;              /* the order's name: the file's name without directories and extension, upper case,
                              its first XB_ORDER_NAME_MAX characters */
  xb_ntx *ntx;
  xb_ntx_info info;
  char type;               /* the key's type: C N D L */
  xb_item key;             /* the key expression's codeblock (a root) */
  xb_item cond;            /* the FOR condition's codeblock, NIL without one (a root) */
  int64_t field;           /* the key is this character field's bytes as they are; -1 otherwise */
  unsigned char old[XB_NTX_KEY_MAX]; /* the current record's key before it changed (xb_area_hot) */
  int oldIn;               /* whether the record had an entry then */
};

/* The key of a value for order `o`: `out` gets keyLen bytes; `*used` the bytes a SEEK compares (a shorter string:
 * its length). 0 when the value's type is not the order's. */
int xb_order_value_key(const xb_order *o, const xb_item *v, unsigned char *out, size_t *used);

/* Builds an order's file from the area's records (the condition's, when it has one; the area's pending ordCondSet()
 * settings apply) and opens it. NULL after the error (raised). */
xb_order *xb_order_create(xb_ctx *ctx, xb_area *a, const char *path, const xb_str *keyText, const xb_item *keyBlock,
                          int unique);
/* Opens an order's file. NULL after the error (raised). */
xb_order *xb_order_open(xb_ctx *ctx, xb_area *a, const char *path);
/* Builds every order's file of the area again from the records, as each describes itself; the first one that
 * cannot be is closed (its error raised), and the ones after it stay as they were. 0 after an error. */
int xb_area_orders_rebuild(xb_ctx *ctx, xb_area *a);
/* Writes what is pending and frees the order. 0 after the error (raised). */
int xb_order_close(xb_ctx *ctx, xb_area *a, xb_order *o);

/* The first (`dir` 1) or last (-1) record of the order, or the one after (before) the current record: 1 with it in
 * `recno`, 0 for none, -1 after an error (raised). */
int xb_order_edge(xb_ctx *ctx, xb_area *a, xb_order *o, int dir, uint32_t *recno);
int xb_order_step(xb_ctx *ctx, xb_area *a, xb_order *o, int dir, uint32_t *recno);
/* The first record whose key starts with `key` (`used` bytes; `last`: the last such), or the first after where it
 * would be: 1 with it, 0 for none, -1 after an error (raised); `*match` whether its key starts so. */
int xb_order_seek(xb_ctx *ctx, xb_area *a, xb_order *o, const unsigned char *key, size_t used, int last,
                  uint32_t *recno, int *match);
/* Whether the key of the entry the cursor is on (the current record's) starts with `key`. */
int xb_order_cursor_matches(xb_order *o, const unsigned char *key, size_t used);

/* Marks the order's items for the collector. */
void xb_order_mark(xb_order *o, int (*mark)(const xb_item *));

#endif
