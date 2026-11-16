/* Shared objects (docs/concurrency.md, "Shared objects"): an object is local while one flow of execution can reach it,
 * and shared (XB_GC_SHARED in its header) once two can; sharing is transitive. A shared object's items are read under
 * a sequence counter (read, check, read again when a writer came between: no lock, no write) and written under a lock;
 * both are those of the object's stripe, a table entry chosen by its address. A writer never reaches a safepoint while
 * it holds the lock (it allocates before taking it), so a reader that waits for a writer never holds up a collection,
 * and an object, which moves only while every thread is stopped, keeps its address from the first read of an operation
 * to its last. A writer that waits for the lock sleeps in xb_blocking_run, where a collection may run: the object stays
 * where it is because the address it holds on its stack pins it (as for any blocking operation, platform.h). */
#ifndef XB_SHARED_H
#define XB_SHARED_H

#include "gc/gc.h"
#include "platform/atomic.h"

static inline int xb_is_shared(const void *obj) {
  return (xb_atomic_load8(&xb_gc_header(obj)->state, XB_RELAXED) & XB_GC_SHARED) != 0;
}

/* Shares what an item refers to, and all it reaches, by the thread that can reach it alone (the object stays local
 * until this marks it): every object gets the bit once, what is shared already stops the walk, and a string loses its
 * room to grow in place (xb_str_share). It allocates nothing. */
void xb_share(const xb_item *it);

/* Reading: `seq` = xb_seq_read_begin(obj), the items copied with xb_seq_load, then xb_seq_read_valid(obj,
 * seq): when false, a writer came between and the reads start again. */
uint32_t xb_seq_read_begin(const void *obj);
int      xb_seq_read_valid(const void *obj, uint32_t seq);
void     xb_seq_load(xb_item *dst, const xb_item *src);
/* The item `src` of a shared object, read whole. */
void     xb_seq_get(const void *obj, xb_item *dst, const xb_item *src);

/* Writing: xb_seq_lock(obj), the items written with xb_seq_store, xb_seq_unlock(obj); nothing in between may
 * allocate or reach a safepoint. A thread holds one stripe at a time, and reads nothing under a sequence counter while
 * it holds one (another object may share the stripe: the reader would wait for itself). */
void     xb_seq_lock(const void *obj);
void     xb_seq_unlock(const void *obj);
void     xb_seq_store(xb_item *dst, const xb_item *src);
/* The item `dst` of a shared object set to `src`, under its lock. */
void     xb_seq_put(const void *obj, xb_item *dst, const xb_item *src);

#endif
