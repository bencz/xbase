#include "xbrt_int.h"

#include "core/shared_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

xb_ctx *xb_ctx_new(void) {
  xb_gc_init();
  xb_shared_init();
  xb_ctx *ctx = calloc(1, sizeof *ctx);
  if (!ctx) xb_gc_fatal("out of memory");
  ctx->set_decimals = 2;
  ctx->set_exact = 0;
  xb_ctx_set_date_format(ctx, "mm/dd/yy", 8);
  ctx->epoch = 1900;
  ctx->century = 0;
  xb_gc_register(ctx);
  xb_set_nil(&ctx->error_block); /* until ErrorSys() sets one (xb_program_run) */
  xb_set_nil(&ctx->break_value);
  xb_set_nil(&ctx->language);
  ctx->areaCurrent = 1;
  for (uint32_t i = 0; i < 2 * XB_MACRO_CACHE; ++i) xb_set_nil(&ctx->macro_cache[i]);
  return ctx;
}

void xb_ctx_free(xb_ctx *ctx) {
  if (!ctx) return;
  xb_gc_unregister(ctx);
  free(ctx->roots);
  free(ctx->memvars);
  free(ctx->privates);
  free(ctx->date_format);
  free(ctx->areas);
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

/* Slow paths of the pending test (xbrt.h). A safepoint request only pauses
 * the task (all mutators stopped for a collection); with one mutator thread
 * there is nothing to wait for, so it is simply acknowledged. xb_pending never
 * collects: a result the error block substituted may wait across it in a home
 * the collector does not see (docs/xir.md, Passes), and C callers may hold
 * items in their own variables. xb_pending_gc also runs destructors, which may
 * collect. */
int32_t xb_pending(xb_ctx *ctx) {
  ctx->pending &= ~(uint32_t)XB_PENDING_SAFEPOINT;
  return (ctx->pending & XB_PENDING_UNWIND) != 0;
}

int32_t xb_pending_gc(xb_ctx *ctx) {
  if (ctx->pending & XB_PENDING_FINALIZE) {
    ctx->pending &= ~(uint32_t)XB_PENDING_FINALIZE;
    xb_classes_finalize(ctx);
  }
  return xb_pending(ctx);
}
void xb_ctx_raise(xb_ctx *ctx, uint32_t what) { ctx->pending |= what; }
int xb_ctx_exitcode(const xb_ctx *ctx) { return ctx->exitcode; }

void xb_abi_check(uint32_t version, uint32_t item_size, uint32_t str_hdr_size, uint32_t block_cells_offset,
                  uint32_t frame_slots_offset, uint32_t ctx_frames_offset, uint32_t ctx_decimals_offset,
                  uint32_t array_items_offset, uint32_t ref_value_offset, uint32_t hash_entries_offset) {
  if (version != XB_ABI_VERSION || item_size != sizeof(xb_item) || str_hdr_size != offsetof(xb_str, data) ||
      block_cells_offset != offsetof(xb_block, cells) || frame_slots_offset != offsetof(xb_frame, slots) ||
      ctx_frames_offset != offsetof(xb_ctx, frames) || ctx_decimals_offset != offsetof(xb_ctx, set_decimals) ||
      array_items_offset != offsetof(xb_array, items) || ref_value_offset != offsetof(xb_ref, value) ||
      hash_entries_offset != offsetof(xb_hash, entries)) {
    char why[384];
    snprintf(why, sizeof why,
             "ABI mismatch (module v%u item=%u str=%u block=%u frame=%u ctx=%u/%u array=%u ref=%u hash=%u, "
             "runtime v%u item=%u str=%u block=%u frame=%u ctx=%u/%u array=%u ref=%u hash=%u)",
             version, item_size, str_hdr_size, block_cells_offset, frame_slots_offset, ctx_frames_offset,
             ctx_decimals_offset, array_items_offset, ref_value_offset, hash_entries_offset, XB_ABI_VERSION,
             (unsigned)sizeof(xb_item), (unsigned)offsetof(xb_str, data), (unsigned)offsetof(xb_block, cells),
             (unsigned)offsetof(xb_frame, slots), (unsigned)offsetof(xb_ctx, frames),
             (unsigned)offsetof(xb_ctx, set_decimals), (unsigned)offsetof(xb_array, items),
             (unsigned)offsetof(xb_ref, value), (unsigned)offsetof(xb_hash, entries));
    xb_gc_fatal(why);
  }
}
