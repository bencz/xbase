/* Task contexts and item frames. */
#ifndef XB_CONTEXT_H
#define XB_CONTEXT_H

#include "gc/gc.h"

/* Item frames live in a segmented stack: a segment is never reallocated,
 * so pointers handed to compiled code stay valid for the frame lifetime.
 * Frames are GC roots. */
typedef struct xb_seg {
  struct xb_seg *prev;
  struct xb_seg *next;      /* spare segment kept for reuse */
  size_t         cap;
  size_t         used;
  xb_item        items[];
} xb_seg;

struct xb_ctx {
  uint32_t pending;         /* [ABI] must stay the first field */
  int32_t  exitcode;
  xb_seg  *seg;
  xb_ctx  *gcNext, *gcPrev; /* registered root sets */
  xb_item *roots[XB_GC_MAX_ROOTS];
  unsigned rootLen;
  /* Later: memvar (PRIVATE) stack, current work area, SETs, ErrorBlock -
   * all per task, so they travel with a Task across threads like .NET's
   * ExecutionContext. */
  int32_t  set_decimals;
  int      set_exact;
};

#define XB_SEG_DEFAULT 4096

/* SET DECIMALS as an item's decimal count (items keep 16 bits, as in Clipper). */
static inline uint16_t xb_decimals(const xb_ctx *ctx) { return (uint16_t)ctx->set_decimals; }

#endif
