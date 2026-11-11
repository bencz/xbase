/* Garbage collector interface (see gc.c). */
#ifndef XB_GC_H
#define XB_GC_H

#include "xbrt.h"

/* Garbage-collected objects. An 8-byte header precedes every object. */
typedef enum xb_gc_kind {
  XB_GC_STRING = 1,
  XB_GC_ARRAY,
  XB_GC_BLOCK,
  XB_GC_CELL,
  XB_GC_ITEMS,              /* the items of an array that outgrew its own storage */
  XB_GC_REF,                /* xb_ref, the target of an XB_T_REF item */
  XB_GC_OBJECT,             /* xb_object */
  XB_GC_HASH,               /* xb_hash */
  XB_GC_HSTORE,             /* the entries, hashes, index and tree of a hash */
} xb_gc_kind;

typedef struct xb_gc_hdr {
  uint32_t size;            /* object bytes: the lines a block object covers, statistics (saturates for large) */
  uint8_t  kind;            /* xb_gc_kind */
  uint8_t  state;           /* XB_GC_MARKED | XB_GC_REMEMBERED */
  uint16_t flags;           /* XB_GC_HDR_LARGE */
} xb_gc_hdr;

/* xb_gc_hdr.flags: allocated on its own, not in a block. */
#define XB_GC_HDR_LARGE 1u

/* Marked by the last collection that saw it, i.e. old (marks are sticky). */
#define XB_GC_MARKED     XB_GC_STATE_OLD
/* An old object in the remembered set, to be traced whole: it may point to
 * young objects anywhere. */
#define XB_GC_REMEMBERED XB_GC_STATE_RECORDED
/* An old array in the remembered set for its dirty cards only. */
#define XB_GC_CARDED     0x4u
/* Young and reached by the young collection in progress. */
#define XB_GC_SEEN       0x8u
/* Young and survived one young collection: the next one that reaches it
 * promotes it. */
#define XB_GC_AGED       0x10u

_Static_assert(sizeof(xb_gc_hdr) - offsetof(xb_gc_hdr, state) == XB_GC_STATE_BACK, "state byte position (xbrt.h)");

static inline xb_gc_hdr *xb_gc_header(const void *obj) { return (xb_gc_hdr *)obj - 1; }

/* Items outside frames that stay alive for the whole run (module STATICs). */
void  xb_gc_add_root(xb_item *items, size_t count); /* may run before the first context */

/* xb_gc_write_item, the barrier for array elements, is in xbrt.h. */

void  xb_gc_init(void);                    /* by the first context, before other threads exist */
void *xb_gc_alloc(xb_gc_kind kind, size_t size);
void  xb_gc_collect(void);                 /* a full collection */
void  xb_gc_collect_young(void);           /* a young collection */
void  xb_gc_register(xb_ctx *ctx);
void  xb_gc_unregister(xb_ctx *ctx);
/* Temporary C roots of a context, as many as nested C calls need (a vector
 * that grows; growing it is the only allocation, and never a collection). */
void  xb_gc_root_push(xb_ctx *ctx, xb_item *it);
void  xb_gc_root_pop(xb_ctx *ctx, unsigned n);
size_t xb_gc_live_objects(void);           /* found by the last full collection */
/* Finalization: an object registered with xb_gc_finalizable that a
 * collection finds unreachable is kept alive, queued, and every context gets
 * XB_PENDING_FINALIZE; xb_gc_next_finalized hands the queued objects out, in
 * the order they were found (1, or 0 when none is left; the caller roots the
 * item, which the queue no longer does). While xb_gc_inhibit(1) is in effect
 * (calls nest, each ended by xb_gc_inhibit(0)), allocation collects nothing. */
void   xb_gc_finalizable(void *obj);
int    xb_gc_next_finalized(xb_item *out);
size_t xb_gc_finalizable_count(void);
void   xb_gc_inhibit(int on);
_Noreturn void xb_gc_fatal(const char *why); /* unrecoverable: reports and ends the process */

#endif
