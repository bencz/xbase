#include "xbrt_int.h"

#include "core/shared_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The environment a program starts with: Clipper's SET values (SET DECIMALS is the thread context's), no ErrorBlock()
 * until ErrorSys() sets one (xb_program_run), work area 1 current. */
static xb_env *env_new(void) {
  xb_env *env = calloc(1, sizeof *env);
  if (!env) xb_gc_fatal("out of memory");
  xb_set_nil(&env->error_block);
  xb_set_nil(&env->language);
  env->epoch = 1900;
  env->areaCurrent = 1;
  return env;
}

static void env_free(xb_env *env) {
  free(env->memvars);
  free(env->privates);
  free(env->date_format);
  free(env->areas);
  free(env);
}

/* The monotonic clock when the first context was made, as the program started: ElapsedMs() counts from it. */
static int64_t started;

static void start_clock(void) { started = xb_platform_monotonic_ns(); }

int64_t xb_program_elapsed_ms(void) { return (xb_platform_monotonic_ns() - started) / XB_NS_PER_MS; }

xb_ctx *xb_ctx_new(void) {
  static xb_once_flag clock = XB_ONCE_INIT;
  xb_once(&clock, start_clock);
  xb_gc_init();
  xb_shared_init();
  xb_ctx *ctx = calloc(1, sizeof *ctx);
  if (!ctx) xb_gc_fatal("out of memory");
  ctx->env = env_new();
  ctx->set_decimals = 2;
  xb_ctx_set_date_format(ctx, "mm/dd/yy", 8);
  xb_set_nil(&ctx->break_value);
  for (uint32_t i = 0; i < 2 * XB_MACRO_CACHE; ++i) xb_set_nil(&ctx->macro_cache[i]);
  xb_gc_register(ctx);
  return ctx;
}

void xb_ctx_free(xb_ctx *ctx) {
  if (!ctx) return;
  xb_gc_unregister(ctx);
  free(ctx->roots);
  env_free(ctx->env);
  free(ctx);
}

/* The stack error leaves its handler the room above `stack_low` + XB_STACK_FLOOR while it runs. */
#define XB_STACK_RESERVE ((size_t)1 << 20)
#define XB_STACK_FLOOR ((size_t)64 << 10)

void xb_ctx_set_stack(xb_ctx *ctx, const char *low) {
  ctx->stack_low = low;
  ctx->stack_limit = low ? low + XB_STACK_RESERVE : NULL;
}

/* Raised before the body of a function whose frame lies below the limit: the error's handler (ErrorSys, a BEGIN
 * SEQUENCE's) runs in the reserve, down to the floor; past it nothing more can be done safely. */
void xb_stack_overflow(xb_ctx *ctx) {
  const char *limit = ctx->stack_limit;
  if (limit == ctx->stack_low + XB_STACK_FLOOR) xb_gc_fatal("stack overflow");
  ctx->stack_limit = ctx->stack_low + XB_STACK_FLOOR;
  const char *name = ctx->frames && ctx->frames->proc ? ctx->frames->proc->name : "";
  xb_error_described(ctx, NULL, EG_LIMIT, 0, "Stack overflow", name, 0, 0, NULL);
  ctx->stack_limit = limit;
}

static int32_t exitCode;

int32_t xb_exit_code(void) { return (int32_t)xb_atomic_load32((uint32_t *)&exitCode, XB_RELAXED); }
void xb_exit_code_set(int32_t code) { xb_atomic_store32((uint32_t *)&exitCode, (uint32_t)code, XB_RELAXED); }

void xb_ctx_set_date_format(xb_ctx *ctx, const char *s, size_t len) {
  char *fmt = malloc(len + 1);
  if (!fmt) xb_gc_fatal("out of memory");
  memcpy(fmt, s, len);
  fmt[len] = '\0';
  free(ctx->env->date_format);
  ctx->env->date_format = fmt;
}

/* Slow paths of the pending test (xbrt.h). xb_pending never collects and never parks the thread for another's
 * collection: a result the error block substituted may wait across it in a home the collector does not see
 * (docs/xir.md, Passes), and C callers may hold items in their own variables; a safepoint request stays pending for the
 * next poll that serves it. xb_pending_gc serves it (it parks the thread while another thread collects) and runs the
 * destructors, which may collect. */
int32_t xb_pending(xb_ctx *ctx) { return (xb_pending_get(ctx) & XB_PENDING_UNWIND) != 0; }

int32_t xb_pending_gc(xb_ctx *ctx) {
  if (xb_pending_get(ctx) & XB_PENDING_SAFEPOINT) {
    xb_pending_clear(ctx, XB_PENDING_SAFEPOINT);
    xb_gc_safepoint();
  }
  if (xb_pending_get(ctx) & XB_PENDING_FINALIZE) {
    xb_pending_clear(ctx, XB_PENDING_FINALIZE);
    xb_classes_finalize(ctx);
  }
  return xb_pending(ctx);
}

void xb_ctx_raise(xb_ctx *ctx, uint32_t what) { xb_pending_set(ctx, what); }

void xb_abi_check(uint32_t version, uint32_t item_size, uint32_t str_hdr_size, uint32_t block_cells_offset,
                  uint32_t frame_slots_offset, uint32_t ctx_frames_offset, uint32_t ctx_decimals_offset,
                  uint32_t ctx_stack_limit_offset, uint32_t array_items_offset, uint32_t ref_value_offset,
                  uint32_t hash_entries_offset) {
  if (version != XB_ABI_VERSION || item_size != sizeof(xb_item) || str_hdr_size != offsetof(xb_str, data) ||
      block_cells_offset != offsetof(xb_block, cells) || frame_slots_offset != offsetof(xb_frame, slots) ||
      ctx_frames_offset != offsetof(xb_ctx, frames) || ctx_decimals_offset != offsetof(xb_ctx, set_decimals) ||
      ctx_stack_limit_offset != offsetof(xb_ctx, stack_limit) ||
      array_items_offset != offsetof(xb_array, items) || ref_value_offset != offsetof(xb_ref, value) ||
      hash_entries_offset != offsetof(xb_hash, entries)) {
    char why[384];
    snprintf(why, sizeof why,
             "ABI mismatch (module v%u item=%u str=%u block=%u frame=%u ctx=%u/%u/%u array=%u ref=%u hash=%u, "
             "runtime v%u item=%u str=%u block=%u frame=%u ctx=%u/%u/%u array=%u ref=%u hash=%u)",
             version, item_size, str_hdr_size, block_cells_offset, frame_slots_offset, ctx_frames_offset,
             ctx_decimals_offset, ctx_stack_limit_offset, array_items_offset, ref_value_offset, hash_entries_offset,
             XB_ABI_VERSION, (unsigned)sizeof(xb_item), (unsigned)offsetof(xb_str, data),
             (unsigned)offsetof(xb_block, cells), (unsigned)offsetof(xb_frame, slots),
             (unsigned)offsetof(xb_ctx, frames), (unsigned)offsetof(xb_ctx, set_decimals),
             (unsigned)offsetof(xb_ctx, stack_limit), (unsigned)offsetof(xb_array, items),
             (unsigned)offsetof(xb_ref, value), (unsigned)offsetof(xb_hash, entries));
    xb_gc_fatal(why);
  }
}
