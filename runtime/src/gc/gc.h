/* Garbage collector interface (see gc.c). */
#ifndef XB_GC_H
#define XB_GC_H

#include "xbrt.h"

/* Garbage-collected objects. An 8-byte header precedes every object. */
typedef enum xb_gc_kind {
  XB_GC_FREE = 0,           /* a free slot of a page */
  XB_GC_STRING,
  XB_GC_ARRAY,
  XB_GC_BLOCK,
  XB_GC_CELL,
  XB_GC_ITEMS,              /* the items of an array that outgrew its own storage */
  XB_GC_REF,                /* xb_ref, the target of an XB_T_REF item */
} xb_gc_kind;

typedef struct xb_gc_hdr {
  uint32_t size;            /* object bytes (statistics) */
  uint8_t  kind;            /* xb_gc_kind */
  uint8_t  marked;
  uint16_t reserved;
} xb_gc_hdr;

static inline xb_gc_hdr *xb_gc_header(const void *obj) { return (xb_gc_hdr *)obj - 1; }

#define XB_GC_MAX_ROOTS 64  /* temporary C roots per context */

/* Items outside frames that stay alive for the whole run (module STATICs). */
void  xb_gc_add_root(xb_item *items, size_t count); /* may run before the first context */

void  xb_gc_init(void);                    /* by the first context, before other threads exist */
void *xb_gc_alloc(xb_gc_kind kind, size_t size);
void  xb_gc_collect(void);
void  xb_gc_register(xb_ctx *ctx);
void  xb_gc_unregister(xb_ctx *ctx);
void  xb_gc_root_push(xb_ctx *ctx, xb_item *it);
void  xb_gc_root_pop(xb_ctx *ctx, unsigned n);
size_t xb_gc_live_objects(void);           /* found by the last collection */
void  xb_gc_fatal(const char *why);        /* unrecoverable: reports and ends the process */

#endif
