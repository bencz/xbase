#include "xbrt_int.h"

#include "core/shared.h"
#include "core/shared_file.h"
#include "rdd/rdd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The environment a program starts with: Clipper's SET values (SET DECIMALS is the thread context's), no ErrorBlock()
 * until ErrorSys() sets one (xb_program_run), work area 1 current. */
/* The thread's spare environment, or a new one (zeroed): its fields are the caller's to set, but its date format's
 * buffer (dateOwn, dateCap). */
static xb_env *env_take(xb_ctx *ctx) {
  xb_env *env = ctx ? ctx->spareEnv : NULL;
  if (env) {
    ctx->spareEnv = NULL;
    return env;
  }
  env = calloc(1, sizeof *env);
  if (!env) xb_gc_fatal("out of memory");
  return env;
}

static xb_env *env_new(xb_ctx *ctx) {
  xb_env *env = env_take(ctx);
  char *fmt = env->dateOwn;
  size_t cap = env->dateCap;
  memset(env, 0, sizeof *env);
  env->dateOwn = fmt;
  env->dateCap = cap;
  xb_set_nil(&env->error_block);
  xb_set_nil(&env->language);
  env->epoch = 1900;
  env->areaCurrent = 1;
  return env;
}

static void env_free(xb_ctx *ctx, xb_env *env) {
  if (env->memvars) free(env->memvars); /* most environments never had these */
  if (env->privates) free(env->privates);
  if (env->areas) free(env->areas);
  env->memvars = NULL;
  env->privates = NULL;
  env->areas = NULL;
  if (ctx && !ctx->spareEnv) {
    ctx->spareEnv = env;
    return;
  }
  free(env->dateOwn);
  free(env);
}

/* The scalar SET values of a snapshot, kept as the bytes of a string: no item refers to the heap. */
typedef struct env_sets {
  int32_t decimals, epoch;
  int exact, fixed, century, deleted, shared, softseek, unique;
  const struct xb_codepage *codepage;
  const struct xb_rdd *rddDefault;
} env_sets;

enum { SNAP_SETS, SNAP_DATE_FORMAT, SNAP_ERROR_BLOCK, SNAP_LANGUAGE, SNAP_MEMVARS, SNAP_COUNT_ };

void xb_env_capture(xb_ctx *ctx, xb_item *r) {
  const xb_env *env = ctx->env;
  xb_array *a = xb_array_alloc(SNAP_COUNT_); /* `a`, on the stack, stays where it is */
  xb_set_array(r, a);
  env_sets sets = {ctx->set_decimals, env->epoch,       env->set_exact,    env->set_fixed, env->century,
                   env->set_deleted,  env->set_shared,  env->set_softseek, env->set_unique, env->codepage,
                   env->rddDefault};
  xb_set_str(&a->items[SNAP_SETS], xb_str_new((const char *)&sets, sizeof sets));
  xb_gc_write(a);
  xb_set_str(&a->items[SNAP_DATE_FORMAT], xb_str_new(env->date_format, strlen(env->date_format)));
  xb_gc_write(a);
  a->items[SNAP_ERROR_BLOCK] = env->error_block;
  a->items[SNAP_LANGUAGE] = env->language;
  xb_gc_write(a);
  xb_item memvars;
  xb_set_nil(&memvars);
  xb_gc_root_push(ctx, &memvars);
  xb_memvar_capture(ctx, &memvars);
  a->items[SNAP_MEMVARS] = memvars;
  xb_gc_write(a);
  xb_gc_root_pop(ctx, 1);
}

void xb_env_restore(xb_ctx *ctx, const xb_item *snapshot) {
  xb_env *env = ctx->env;
  xb_item it;
  env_sets sets;
  xb_array_get(snapshot->v.p, SNAP_SETS, &it);
  memcpy(&sets, it.v.s->data, sizeof sets);
  ctx->set_decimals = sets.decimals;
  env->epoch = sets.epoch;
  env->set_exact = sets.exact;
  env->set_fixed = sets.fixed;
  env->century = sets.century;
  env->set_deleted = sets.deleted;
  env->set_shared = sets.shared;
  env->set_softseek = sets.softseek;
  env->set_unique = sets.unique;
  env->codepage = sets.codepage;
  env->rddDefault = sets.rddDefault;
  xb_array_get(snapshot->v.p, SNAP_DATE_FORMAT, &it);
  xb_ctx_set_date_format(ctx, it.v.s->data, it.v.s->len);
  xb_array_get(snapshot->v.p, SNAP_ERROR_BLOCK, &env->error_block);
  xb_array_get(snapshot->v.p, SNAP_LANGUAGE, &env->language);
  xb_array_get(snapshot->v.p, SNAP_MEMVARS, &it);
  xb_memvar_restore(ctx, &it);
}

void xb_env_enter(xb_ctx *ctx, const xb_item *snapshot) {
  xb_env *env = env_new(ctx);
  env->outer = ctx->env;
  env->outerDecimals = ctx->set_decimals;
  ctx->env = env;
  xb_env_restore(ctx, snapshot);
}

xb_env *xb_env_inherit(xb_ctx *ctx) {
  xb_env *from = ctx->env, *env = env_take(ctx);
  char *fmt = env->dateOwn;
  size_t cap = env->dateCap;
  *env = *from; /* the SET values, ErrorBlock(), the language, the code page, the default driver; the rest below */
  env->dateOwn = fmt;
  env->dateCap = cap;
  env->memvars = NULL;
  env->memvarCap = 0;
  env->privates = NULL;
  env->privateLen = env->privateCap = 0;
  env->file_error = 0;
  env->areas = NULL;
  env->areaCap = 0;
  env->areaCurrent = 1;
  env->netErr = 0;
  env->parkedTask = NULL;
  env->parkedPrev = env->parkedNext = NULL;
  xb_memvar_inherit(env, from);
  env->outer = from;
  env->outerDecimals = ctx->set_decimals;
  ctx->env = env;
  return env;
}

/* A borrowed date format becomes the environment's own: it outlives the one it was borrowed from. */
static void own_date_format(xb_ctx *ctx) {
  if (ctx->env->date_format != ctx->env->dateOwn)
    xb_ctx_set_date_format(ctx, ctx->env->date_format, strlen(ctx->env->date_format));
}

/* The parked environments (a list under its lock), and those whose tasks died, to close (`dead`, the same lock;
 * `deaths` tells a thread there are some without the lock). */
static xb_mutex parkedLock;
static xb_once_flag parkedMade = XB_ONCE_INIT;
static xb_env *parked, *dead;
static uint32_t deaths;

static void make_parked(void) {
  if (!xb_mutex_init(&parkedLock)) xb_gc_fatal("cannot create the environments' lock");
}

static void unlink_parked(xb_env *env) {
  if (env->parkedPrev) env->parkedPrev->parkedNext = env->parkedNext;
  else parked = env->parkedNext;
  if (env->parkedNext) env->parkedNext->parkedPrev = env->parkedPrev;
  env->parkedPrev = env->parkedNext = NULL;
}

void xb_env_park(xb_ctx *ctx, xb_env *env, void *task) {
  xb_once(&parkedMade, make_parked);
  own_date_format(ctx);
  env->parkedDecimals = ctx->set_decimals;
  ctx->env = env->outer;
  ctx->set_decimals = env->outerDecimals;
  env->outer = NULL;
  env->parkedTask = task;
  xb_mutex_lock(&parkedLock); /* no safepoint until it is released: no collection meanwhile */
  env->parkedPrev = NULL;
  env->parkedNext = parked;
  if (parked) parked->parkedPrev = env;
  parked = env;
  xb_mutex_unlock(&parkedLock);
}

void xb_env_unpark(xb_ctx *ctx, xb_env *env) {
  xb_mutex_lock(&parkedLock);
  unlink_parked(env);
  xb_mutex_unlock(&parkedLock);
  env->parkedTask = NULL;
  env->outer = ctx->env;
  env->outerDecimals = ctx->set_decimals;
  ctx->env = env;
  ctx->set_decimals = env->parkedDecimals;
}

void xb_env_parked_each(void (*fn)(xb_env *env)) {
  for (xb_env *env = parked; env; env = env->parkedNext) fn(env);
  for (xb_env *env = dead; env; env = env->parkedNext) fn(env);
}

void xb_env_parked_settle(void *(*live)(void *obj)) {
  for (xb_env *env = parked, *next; env; env = next) {
    next = env->parkedNext;
    env->parkedTask = live(env->parkedTask);
    if (env->parkedTask) continue;
    unlink_parked(env);
    env->parkedNext = dead;
    if (dead) dead->parkedPrev = env;
    dead = env;
    xb_atomic_store32(&deaths, 1, XB_RELEASE);
  }
}

/* Closes the environments of `list`, one at a time `ctx`'s (its work areas closed as a flow's at its end). */
static void close_parked(xb_ctx *ctx, int all) {
  if (!all && !xb_atomic_load32(&deaths, XB_ACQUIRE)) return;
  xb_once(&parkedMade, make_parked);
  for (;;) {
    xb_mutex_lock(&parkedLock);
    xb_env *env = dead ? dead : all ? parked : NULL;
    if (env == dead && env) {
      dead = env->parkedNext;
      if (dead) dead->parkedPrev = NULL;
      env->parkedNext = NULL;
    } else if (env) {
      unlink_parked(env);
    }
    if (!env) xb_atomic_store32(&deaths, 0, XB_RELAXED);
    xb_mutex_unlock(&parkedLock);
    if (!env) return;
    env->parkedTask = NULL;
    env->outer = ctx->env;
    env->outerDecimals = ctx->set_decimals;
    ctx->env = env;
    ctx->set_decimals = env->parkedDecimals;
    xb_env_leave(ctx);
  }
}

void xb_env_reap(xb_ctx *ctx) { close_parked(ctx, 0); }

void xb_env_reap_all(xb_ctx *ctx) { close_parked(ctx, 1); }

void xb_env_close_areas(xb_ctx *ctx) {
  uint32_t open = 0;
  for (uint32_t n = 0; n < ctx->env->areaCap && !open; ++n) open = ctx->env->areas[n] != NULL;
  if (!open) return;
  uint32_t requests = xb_pending_get(ctx) & XB_PENDING_UNWIND;
  xb_item value = ctx->break_value;
  xb_gc_root_push(ctx, &value);
  xb_pending_clear(ctx, requests);
  xb_area_close_all(ctx);
  xb_pending_clear(ctx, XB_PENDING_UNWIND & ~(uint32_t)XB_PENDING_QUIT); /* what closing raised; a QUIT stays */
  xb_pending_set(ctx, requests);
  ctx->break_value = value;
  xb_gc_root_pop(ctx, 1);
}

void xb_env_leave(xb_ctx *ctx) {
  xb_env *env = ctx->env;
  xb_env_close_areas(ctx);
  ctx->env = env->outer;
  ctx->set_decimals = env->outerDecimals;
  env_free(ctx, env);
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
  ctx->env = env_new(NULL);
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
  env_free(NULL, ctx->env);
  if (ctx->spareEnv) env_free(NULL, ctx->spareEnv);
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
  xb_env *env = ctx->env;
  if (len + 1 > env->dateCap) { /* `s` is not in the buffer then: the buffer holds no text that long */
    char *fmt = malloc(len + 1);
    if (!fmt) xb_gc_fatal("out of memory");
    free(env->dateOwn);
    env->dateOwn = fmt;
    env->dateCap = len + 1;
  }
  memmove(env->dateOwn, s, len);
  env->dateOwn[len] = '\0';
  env->date_format = env->dateOwn;
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
