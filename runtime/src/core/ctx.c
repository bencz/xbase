#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

xb_ctx *xb_ctx_new(void) {
  xb_gc_init();
  xb_ctx *ctx = calloc(1, sizeof *ctx);
  if (!ctx) xb_gc_fatal("out of memory");
  ctx->set_decimals = 2;
  ctx->set_exact = 0;
  xb_ctx_set_date_format(ctx, "mm/dd/yy", 8);
  ctx->epoch = 1900;
  ctx->century = 0;
  xb_gc_register(ctx);
  return ctx;
}

void xb_ctx_free(xb_ctx *ctx) {
  if (!ctx) return;
  xb_gc_unregister(ctx);
  free(ctx->date_format);
  free(ctx);
}

void xb_ctx_set_date_format(xb_ctx *ctx, const char *s, size_t len) {
  char *fmt = malloc(len + 1);
  if (!fmt) xb_gc_fatal("out of memory");
  memcpy(fmt, s, len);
  fmt[len] = '\0';
  free(ctx->date_format);
  ctx->date_format = fmt;
}

uint32_t xb_ctx_pending(const xb_ctx *ctx) { return ctx->pending; }

/* Slow path of the pending test in compiled code. A safepoint request only
 * pauses the task (all mutators stopped for a collection); with one mutator
 * thread there is nothing to wait for, so it is simply acknowledged. */
int32_t xb_pending(xb_ctx *ctx) {
  ctx->pending &= ~(uint32_t)XB_PENDING_SAFEPOINT;
  return (ctx->pending & XB_PENDING_UNWIND) != 0;
}
void xb_ctx_raise(xb_ctx *ctx, uint32_t what) { ctx->pending |= what; }
int xb_ctx_exitcode(const xb_ctx *ctx) { return ctx->exitcode; }

void xb_abi_check(uint32_t version, uint32_t item_size, uint32_t str_hdr_size, uint32_t block_cells_offset,
                  uint32_t frame_slots_offset, uint32_t ctx_frames_offset) {
  if (version != XB_ABI_VERSION || item_size != sizeof(xb_item) || str_hdr_size != offsetof(xb_str, data) ||
      block_cells_offset != offsetof(xb_block, cells) || frame_slots_offset != offsetof(xb_frame, slots) ||
      ctx_frames_offset != offsetof(xb_ctx, frames)) {
    char why[256];
    snprintf(why, sizeof why,
             "ABI mismatch (module v%u item=%u str=%u block=%u frame=%u ctx=%u, "
             "runtime v%u item=%u str=%u block=%u frame=%u ctx=%u)",
             version, item_size, str_hdr_size, block_cells_offset, frame_slots_offset, ctx_frames_offset,
             XB_ABI_VERSION, (unsigned)sizeof(xb_item), (unsigned)offsetof(xb_str, data),
             (unsigned)offsetof(xb_block, cells), (unsigned)offsetof(xb_frame, slots),
             (unsigned)offsetof(xb_ctx, frames));
    xb_gc_fatal(why);
  }
}

void xb_rt_error(xb_ctx *ctx, const char *subsystem, int code, const char *desc,
                 const char *operation) {
  /* Until ErrorBlock()/BEGIN SEQUENCE exist, every error is fatal. */
  fflush(stdout);
  fprintf(stderr, "\nError %s/%d  %s", subsystem, code, desc);
  if (operation && *operation) fprintf(stderr, ": %s", operation);
  fputc('\n', stderr);
  ctx->exitcode = 1;
  ctx->pending |= XB_PENDING_ERROR;
}
