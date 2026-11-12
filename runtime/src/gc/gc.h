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
  uint16_t flags;           /* XB_GC_HDR_LARGE, XB_GC_HDR_PINNED, XB_GC_HDR_MOVED */
} xb_gc_hdr;

/* xb_gc_hdr.flags: allocated on its own, not in a block (never moves); pinned by the collection in progress (a word of
 * the stack, or a C root's address, points into it: it does not move); moved by it (the object holds the copy's
 * address, see gc.c). */
#define XB_GC_HDR_LARGE 1u
#define XB_GC_HDR_PINNED 2u
#define XB_GC_HDR_MOVED 4u

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

void  xb_gc_init(void);                    /* by every context made: the heap is made once */
/* Threads. A thread that runs xBase code attaches first and detaches before it ends; the end of its stack is the one
 * the system reports (xb_platform_stack_bounds), else `base`, an address in the frame of its first function (NULL:
 * unknown, and then no collection moves objects). A collection stops every attached thread (each one parks at its next
 * safepoint, or is blocked) and scans their stacks for the objects to pin. xb_gc_safepoint parks the caller while
 * another thread's collection runs: compiled code's polls of XB_PENDING_SAFEPOINT and the allocations' slow paths call
 * it, where every object still needed is rooted. xb_blocking_run (platform.h) runs what may block the thread. */
void  xb_gc_thread_attach(const void *base);
void  xb_gc_thread_detach(void);
void  xb_gc_safepoint(void);
/* While held, collections move nothing: for C code that keeps object addresses where no scan sees them (a map of
 * addresses in malloc'd memory). */
void  xb_gc_hold(void);
void  xb_gc_release(void);
void *xb_gc_alloc(xb_gc_kind kind, size_t size);
void  xb_gc_collect(void);                 /* a full collection */
void  xb_gc_collect_young(void);           /* a young collection */
void  xb_gc_register(xb_ctx *ctx);
void  xb_gc_unregister(xb_ctx *ctx);
size_t xb_gc_live_objects(void);           /* found by the last full collection */
/* The barrier after storing items `from` .. `to` - 1 of an array (xb_gc_write_item for each card they cover). */
void   xb_gc_write_items(xb_array *a, size_t from, size_t to);
/* Finalization: an object registered with xb_gc_finalizable that a
 * collection finds unreachable is kept alive, queued, and every context gets
 * XB_PENDING_FINALIZE; xb_gc_next_finalized hands the queued objects out, in
 * the order they were found (1, or 0 when none is left; the caller roots the
 * item, which the queue no longer does). */
void   xb_gc_finalizable(void *obj);
int    xb_gc_next_finalized(xb_item *out);
size_t xb_gc_finalizable_count(void);    /* registered and not yet handed out */
_Noreturn void xb_gc_fatal(const char *why); /* unrecoverable: reports and ends the process */

#endif
