/* Task contexts. */
#ifndef XB_CONTEXT_H
#define XB_CONTEXT_H

#include "gc/gc.h"

struct xb_ctx {
  uint32_t  pending;        /* [ABI] the first four fields, in this order (see xbrt.h) */
  int32_t   exitcode;
  xb_frame *frames;         /* innermost frame of compiled code; frames are GC roots */
  int32_t   set_decimals;   /* SET DECIMALS: compiled code reads it for / and % */
  xb_ctx  *gcNext, *gcPrev; /* registered root sets */
  xb_item **roots;          /* temporary C roots (xb_gc_root_push) */
  unsigned rootLen, rootCap;
  /* ErrorBlock(): a codeblock, NIL until ErrorSys() runs (a root of the collector, like the next one) */
  xb_item  error_block;
  xb_item  break_value;     /* the value of a pending BREAK */
  uint32_t error_depth;     /* error blocks running */
  /* Later: memvar (PRIVATE) stack, current work area, SETs - all per task, so
   * they travel with a Task across threads like .NET's ExecutionContext. */
  int      set_exact;
  char    *date_format;     /* SET DATE FORMAT picture (malloc'ed, NUL-terminated) */
  int32_t  epoch;           /* SET EPOCH */
  int      century;         /* SET CENTURY */
};

void xb_ctx_set_date_format(xb_ctx *ctx, const char *s, size_t len);

/* A runtime function shows on the call stack while what it does can be seen
 * (a codeblock it calls, an error block it runs): a frame without slots,
 * linked from push to pop (see xb_frame in xbrt.h). */
static inline void xb_frame_push(xb_ctx *ctx, xb_frame *f, const xb_proc *proc) {
  f->prev = ctx->frames;
  f->proc = proc;
  f->nslots = 0;
  f->line = 0;
  f->argv = NULL;
  f->argc = 0;
  ctx->frames = f;
}
static inline void xb_frame_pop(xb_ctx *ctx, const xb_frame *f) { ctx->frames = f->prev; }

/* Whether the call stack (ProcName(), the "Called from" lines) shows a frame. */
static inline int xb_frame_shown(const xb_frame *f) { return f->proc && !(f->proc->flags & XB_PROC_HIDDEN); }

/* SET DECIMALS as an item's decimal count (items keep 16 bits, as in Clipper). */
static inline uint16_t xb_decimals(const xb_ctx *ctx) { return (uint16_t)ctx->set_decimals; }

#endif
