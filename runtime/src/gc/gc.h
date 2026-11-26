/* Garbage collector interface (see gc.c). */
#ifndef XB_GC_H
#define XB_GC_H

#include "xbrt.h"

/* xb_gc_hdr.flags: allocated on its own, not in a block (never moves); moved by the collection in progress (the object
 * holds the copy's address, see gc.c). */
#define XB_GC_HDR_LARGE 1u
#define XB_GC_HDR_MOVED 4u
/* The full collections' mark, whose meaning alternates (gc.c, reached_full): during a full collection, equal to its
 * polarity when it reached the object; between full collections, the last one's polarity on what it reached and on
 * what was made since (a new object has it). */
#define XB_GC_HDR_FULL 8u

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
/* Reachable by more than one flow of execution: its items are read under a
 * sequence counter and written under a lock (core/shared.h). Kept by every
 * collection. */
#define XB_GC_SHARED     XB_GC_STATE_SHARED

_Static_assert(sizeof(xb_gc_hdr) - offsetof(xb_gc_hdr, state) == XB_GC_STATE_BACK, "state byte position (xbrt.h)");

static inline xb_gc_hdr *xb_gc_header(const void *obj) { return (xb_gc_hdr *)obj - 1; }

/* Items outside frames that stay alive for the whole run (module STATICs). */
void  xb_gc_add_root(xb_item *items, size_t count); /* may run before the first context */

/* xb_gc_write_item and xb_gc_write_items, the barriers for array elements, are in xbrt.h. */

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
/* xb_gc_alloc for a size the program chose (Space( n ), Array( n )): NULL, rather than the program's end, when a large
 * object (above the largest size class) finds no memory even after a full collection, so that the caller raises an
 * error the program handles; a smaller one that finds none ends the program as xb_gc_alloc does. */
void *xb_gc_try_alloc(xb_gc_kind kind, size_t size);
/* An object that now uses only its first `size` bytes gives the rest back to the system when it is a large one (1);
 * 0 for another (its memory stays). Its address does not change. */
int   xb_gc_trim(void *obj, size_t size);
void  xb_gc_collect(void);                 /* a full collection */
void  xb_gc_collect_young(void);           /* a young collection */
void  xb_gc_register(xb_ctx *ctx);
void  xb_gc_unregister(xb_ctx *ctx);
/* Raises the requests `bits` on every registered context but `except` (the program's QUIT from any thread). */
void  xb_gc_raise_all(const xb_ctx *except, uint32_t bits);
size_t xb_gc_live_objects(void);           /* found by the last full collection */
size_t xb_gc_live_bytes(void);             /* the bytes of those objects */
size_t xb_gc_young_budget(void);           /* the allocation that starts the next young collection */
size_t xb_gc_heap_bytes(void);
size_t xb_gc_occupied_bytes(void);         /* xb_gc_heap_bytes without the free blocks kept for allocations */
/* Bytes allocated since the program started: every thread's reported ones and the calling thread's not yet reported
 * (another thread's count at its collections, slow allocations and detaches). */
uint64_t xb_gc_allocated_bytes(void);
/* The program ends: the trimmer (gc.c, Memory given back) stops, waited for. */
void xb_gc_trim_end(void);
/* Whether the heap's books agree (tests, after a collection): every free block in use listed as free, no block given
 * back or never handed out among those in use, the counts of spare blocks right. */
int    xb_gc_consistent(void);             /* the heap's memory the system has given it now (blocks in use, large) */
/* Finalization: an object registered with xb_gc_finalizable that a
 * collection finds unreachable is kept alive, queued, and every context gets
 * XB_PENDING_FINALIZE; xb_gc_next_finalized hands the queued objects out, in
 * the order they were found (1, or 0 when none is left; the caller roots the
 * item, which the queue no longer does). */
void   xb_gc_finalizable(void *obj);
/* Destructors run on one context at a time: xb_gc_finalizing(1) takes the turn (1, or 0 when another context has it),
 * xb_gc_finalizing(0) gives it back: 1 when objects wait to be finalized still (queued meanwhile). */
int    xb_gc_finalizing(int start);
int    xb_gc_next_finalized(xb_item *out);
size_t xb_gc_finalizable_count(void);    /* registered and not yet handed out */
_Noreturn void xb_gc_fatal(const char *why); /* unrecoverable: reports and ends the process */

#endif
