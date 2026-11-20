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
  xb_memvar_bindings_free(env);
  for (uint32_t i = 0; i < env->privateLen; ++i) /* an environment that ends with PRIVATEs alive: a task abandoned */
    if (env->privates[i].id != XB_PRIVATE_CLEARED) xb_memvar_unhold(env->privates[i].id);
  env->privateLen = 0;
  if (env->privates) free(env->privates); /* most environments never had these */
  if (env->areas) free(env->areas);
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
  xb_set_str(&xb_array_items(a)[SNAP_SETS], xb_str_new((const char *)&sets, sizeof sets));
  xb_gc_write(a);
  xb_set_str(&xb_array_items(a)[SNAP_DATE_FORMAT], xb_str_new(env->date_format, strlen(env->date_format)));
  xb_gc_write(a);
  xb_array_items(a)[SNAP_ERROR_BLOCK] = env->error_block;
  xb_array_items(a)[SNAP_LANGUAGE] = env->language;
  xb_gc_write(a);
  xb_item memvars;
  xb_set_nil(&memvars);
  xb_gc_root_push(ctx, &memvars);
  xb_memvar_capture(ctx, &memvars);
  xb_array_items(a)[SNAP_MEMVARS] = memvars;
  xb_gc_write(a);
  xb_gc_root_pop(ctx, 1);
}

void xb_env_restore(xb_ctx *ctx, const xb_item *snapshot) {
  xb_env *env = ctx->env;
  xb_item it;
  env_sets sets;
  xb_array_get(snapshot->v.p, SNAP_SETS, &it);
  memcpy(&sets, xb_str_data(it.v.s), sizeof sets);
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
  xb_ctx_set_date_format(ctx, xb_str_data(it.v.s), it.v.s->len);
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
  env->bindings = NULL;
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

/* The parked environments, in PARK_SHARDS shards by the environment's address, each a lock and two lists: so that
 * the suspensions and resumptions of independent functions do not wait for one another. A shard's young list holds the
 * environments parked lately, which every collection scans; one that young collections scanned twice moves to the old
 * list (all it reaches is old by then, and a parked environment does not change), which only full collections scan:
 * a young collection costs what was parked lately, whatever waits for long. Those whose tasks died wait in `dead`
 * (under deadLock; `deaths` tells a thread there are some without the lock) until they are closed. */
#define PARK_SHARDS 64u
#define PARK_SCANS_YOUNG 2u
typedef struct park_shard {
  uint32_t lock; /* xb_word_lock: taken at every suspension and resumption, seldom waited for */
  xb_env *young, *old;
} park_shard;
static park_shard shards[PARK_SHARDS];
static xb_mutex deadLock;
static xb_once_flag parkedMade = XB_ONCE_INIT;
static xb_env *dead;
static uint32_t deaths;
static uint32_t parkedReady; /* the locks made: a collection before any park has none to look at */

static void make_parked(void) {
  if (!xb_mutex_init(&deadLock)) xb_gc_fatal("cannot create the environments' lock");
  xb_atomic_store32(&parkedReady, 1, XB_RELEASE);
}

static xb_env **list_of(xb_env *env) {
  park_shard *s = &shards[env->parkedShard];
  return env->parkedOld ? &s->old : &s->young;
}

static void unlink_parked(xb_env *env) {
  if (env->parkedPrev) env->parkedPrev->parkedNext = env->parkedNext;
  else *list_of(env) = env->parkedNext;
  if (env->parkedNext) env->parkedNext->parkedPrev = env->parkedPrev;
  env->parkedPrev = env->parkedNext = NULL;
}

static void link_parked(xb_env **list, xb_env *env) {
  env->parkedPrev = NULL;
  env->parkedNext = *list;
  if (*list) (*list)->parkedPrev = env;
  *list = env;
}

void xb_env_park(xb_ctx *ctx, xb_env *env, void *task) {
  xb_once(&parkedMade, make_parked);
  own_date_format(ctx);
  env->parkedDecimals = ctx->set_decimals;
  ctx->env = env->outer;
  ctx->set_decimals = env->outerDecimals;
  env->outer = NULL;
  env->parkedTask = task;
  env->parkedShard = (uint8_t)(((uintptr_t)env >> 6) % PARK_SHARDS);
  env->parkedOld = 0;
  env->parkedScans = 0;
  park_shard *s = &shards[env->parkedShard];
  xb_word_lock(&s->lock); /* no safepoint until it is released: no collection meanwhile */
  link_parked(&s->young, env);
  xb_word_unlock(&s->lock);
}

void xb_env_unpark(xb_ctx *ctx, xb_env *env) {
  park_shard *s = &shards[env->parkedShard];
  xb_word_lock(&s->lock);
  unlink_parked(env);
  xb_word_unlock(&s->lock);
  env->parkedTask = NULL;
  env->outer = ctx->env;
  env->outerDecimals = ctx->set_decimals;
  ctx->env = env;
  ctx->set_decimals = env->parkedDecimals;
}

void xb_env_parked_each(void (*fn)(xb_env *env), int full) {
  if (!xb_atomic_load32(&parkedReady, XB_ACQUIRE)) return; /* none parked yet */
  for (uint32_t k = 0; k < PARK_SHARDS; ++k) {
    for (xb_env *env = shards[k].young; env; env = env->parkedNext) fn(env);
    if (full)
      for (xb_env *env = shards[k].old; env; env = env->parkedNext) fn(env);
  }
  for (xb_env *env = dead; env; env = env->parkedNext) fn(env);
}

/* An environment of a shard's list (`old`) after a collection's marking: dead to `dead`; a young one scanned twice
 * to the old list. */
static int settle_one(xb_env *env, void *(*live)(void *obj), int old) {
  env->parkedTask = live(env->parkedTask);
  if (!env->parkedTask) {
    unlink_parked(env);
    env->parkedNext = dead;
    if (dead) dead->parkedPrev = env;
    dead = env;
    return 1;
  }
  if (!old && ++env->parkedScans >= PARK_SCANS_YOUNG) {
    unlink_parked(env);
    env->parkedOld = 1;
    link_parked(&shards[env->parkedShard].old, env);
  }
  return 0;
}

int xb_env_parked_settle(void *(*live)(void *obj), int full) {
  if (!xb_atomic_load32(&parkedReady, XB_ACQUIRE)) return 0;
  int died = 0;
  for (uint32_t k = 0; k < PARK_SHARDS; ++k) {
    for (xb_env *env = shards[k].young, *next; env; env = next) {
      next = env->parkedNext;
      died |= settle_one(env, live, 0);
    }
    if (full)
      for (xb_env *env = shards[k].old, *next; env; env = next) {
        next = env->parkedNext;
        died |= settle_one(env, live, 1);
      }
  }
  if (died) xb_atomic_store32(&deaths, 1, XB_RELEASE);
  return died;
}

/* Closes the environments of `list`, one at a time `ctx`'s (its work areas closed as a flow's at its end). */
static void close_parked(xb_ctx *ctx, int all) {
  if (!all && !xb_atomic_load32(&deaths, XB_ACQUIRE)) return;
  xb_once(&parkedMade, make_parked);
  for (uint32_t k = 0;;) {
    xb_mutex_lock(&deadLock);
    xb_env *env = dead;
    if (env) {
      dead = env->parkedNext;
      if (dead) dead->parkedPrev = NULL;
      env->parkedNext = NULL;
    } else {
      xb_atomic_store32(&deaths, 0, XB_RELAXED);
    }
    xb_mutex_unlock(&deadLock);
    while (!env && all && k < PARK_SHARDS) { /* the program's end: every parked one too */
      park_shard *s = &shards[k];
      xb_word_lock(&s->lock);
      env = s->young ? s->young : s->old;
      if (env) unlink_parked(env);
      xb_word_unlock(&s->lock);
      if (!env) ++k;
    }
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
  for (uint32_t i = 0; i < XB_NAME_CACHE; ++i) ctx->nameIds[i] = UINT32_MAX;
  xb_gc_register(ctx);
  return ctx;
}

/* The context whose stack the thread runs on (xb_stack_give_back). */
static XB_THREAD_LOCAL xb_ctx *stackOwner;

void xb_ctx_free(xb_ctx *ctx) {
  if (!ctx) return;
  if (stackOwner == ctx) stackOwner = NULL; /* freed by its own thread */
  xb_gc_unregister(ctx);
  for (uint32_t i = 0; i < XB_NAME_CACHE; ++i)
    if (ctx->nameIds[i] != UINT32_MAX) xb_memvar_unhold(ctx->nameIds[i]);
  free(ctx->roots);
  env_free(NULL, ctx->env);
  if (ctx->spareEnv) env_free(NULL, ctx->spareEnv);
  free(ctx);
}

/* The stack error leaves its handler the room above `stack_low` + XB_STACK_FLOOR while it runs. */
#define XB_STACK_RESERVE ((size_t)1 << 20)
#define XB_STACK_FLOOR ((size_t)64 << 10)
/* The room above the stack error's limit a waiter needs to run a task inline, on top of its own calls. */
#define XB_STACK_INLINE ((size_t)1 << 20)
/* The part of a stack below its end that stays the thread's: past it the stack is deep (its mark), and its pages go
 * back to the system once the thread is XB_STACK_GIVE_ROOM above the mark again (room for the calls that give them). */
#define XB_STACK_KEEP ((size_t)1 << 20)
#define XB_STACK_GIVE_ROOM ((size_t)64 << 10)
/* A thread gives its stack's pages back at most once in this time: one that goes past the mark again and again (a loop
 * that recurses a little deeper) pays the system's call and the pages' faults that often at most, staying deep between. */
#define XB_STACK_GIVE_EVERY_NS ((int64_t)100 * 1000 * 1000)

/* The stack error's limit: where the stack ends for the program's calls. */
static const char *error_limit(const xb_ctx *ctx) { return ctx->stack_low + XB_STACK_RESERVE; }

void xb_ctx_set_stack(xb_ctx *ctx, const char *low, const char *high) {
  ctx->stack_low = low;
  ctx->stack_mark = NULL;
  ctx->stack_limit = NULL;
  if (!low) return;
  ctx->stack_limit = error_limit(ctx);
  stackOwner = ctx;
  /* a mark only on a stack with room below it (the program's, ThreadStart()'s, the workers' of 4 MB); a stack too small
   * for both keeps its pages, and one smaller than the reserve has the error at once */
  if (high && (uintptr_t)high > (uintptr_t)ctx->stack_limit &&
      (uintptr_t)high - (uintptr_t)ctx->stack_limit > 2 * XB_STACK_KEEP) {
    ctx->stack_mark = high - XB_STACK_KEEP;
    ctx->stack_limit = ctx->stack_mark;
  }
}

int xb_stack_room(const xb_ctx *ctx) {
  /* where the thread's stack is now (it grows down, as the limit's check takes it to); in the reserve, no room */
  char here;
  return !ctx->stack_low || (uintptr_t)&here >= (uintptr_t)error_limit(ctx) + XB_STACK_INLINE;
}

int32_t xb_stack_deep(xb_ctx *ctx, uintptr_t frame) {
  if (!ctx->stack_mark || ctx->stack_limit != ctx->stack_mark) return 0;
  ctx->stack_limit = error_limit(ctx);
  return frame >= (uintptr_t)ctx->stack_limit;
}

void xb_stack_give_back(void) {
  xb_ctx *ctx = stackOwner;
  if (!ctx || !ctx->stack_mark || ctx->stack_limit != error_limit(ctx)) return; /* not deep, or in the stack error */
  char here;
  if ((uintptr_t)&here < (uintptr_t)ctx->stack_mark + XB_STACK_GIVE_ROOM) return; /* still deep */
  int64_t now = xb_platform_monotonic_ns();
  if (ctx->stack_given && now - ctx->stack_given < XB_STACK_GIVE_EVERY_NS) return;
  ctx->stack_given = now;
  uintptr_t page = xb_platform_page_size();
  uintptr_t from = ((uintptr_t)ctx->stack_low + page - 1) & ~(page - 1), to = (uintptr_t)ctx->stack_mark & ~(page - 1);
  if (to > from) xb_platform_stack_give_back((const char *)from, (const char *)to);
  ctx->stack_limit = ctx->stack_mark;
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
  if (xb_pending_get(ctx) & XB_PENDING_FINALIZE) { /* destructors, and the environments of tasks that died */
    xb_pending_clear(ctx, XB_PENDING_FINALIZE);
    xb_classes_finalize(ctx);
    xb_env_reap(ctx);
  }
  return xb_pending(ctx);
}

void xb_ctx_raise(xb_ctx *ctx, uint32_t what) { xb_pending_set(ctx, what); }

void xb_abi_check(uint32_t version, uint32_t item_size, uint32_t str_hdr_size, uint32_t block_cells_offset,
                  uint32_t frame_slots_offset, uint32_t ctx_frames_offset, uint32_t ctx_decimals_offset,
                  uint32_t ctx_stack_limit_offset, uint32_t array_data_offset, uint32_t ref_value_offset,
                  uint32_t hash_entries_offset, uint32_t object_vars_offset, uint32_t strbuf_bytes_offset) {
  if (version != XB_ABI_VERSION || item_size != sizeof(xb_item) || str_hdr_size != offsetof(xb_str, bytes) ||
      block_cells_offset != offsetof(xb_block, cells) || frame_slots_offset != offsetof(xb_frame, slots) ||
      ctx_frames_offset != offsetof(xb_ctx, frames) || ctx_decimals_offset != offsetof(xb_ctx, set_decimals) ||
      ctx_stack_limit_offset != offsetof(xb_ctx, stack_limit) ||
      array_data_offset != offsetof(xb_array, data) || ref_value_offset != offsetof(xb_ref, value) ||
      hash_entries_offset != offsetof(xb_hash, entries) || object_vars_offset != offsetof(xb_object, vars) ||
      strbuf_bytes_offset != offsetof(xb_strbuf, bytes)) {
    char why[512];
    snprintf(why, sizeof why,
             "ABI mismatch (module v%u item=%u str=%u block=%u frame=%u ctx=%u/%u/%u array=%u ref=%u hash=%u "
             "object=%u strbuf=%u, runtime v%u item=%u str=%u block=%u frame=%u ctx=%u/%u/%u array=%u ref=%u hash=%u "
             "object=%u strbuf=%u)",
             version, item_size, str_hdr_size, block_cells_offset, frame_slots_offset, ctx_frames_offset,
             ctx_decimals_offset, ctx_stack_limit_offset, array_data_offset, ref_value_offset, hash_entries_offset,
             object_vars_offset, strbuf_bytes_offset,
             XB_ABI_VERSION, (unsigned)sizeof(xb_item), (unsigned)offsetof(xb_str, bytes),
             (unsigned)offsetof(xb_block, cells), (unsigned)offsetof(xb_frame, slots),
             (unsigned)offsetof(xb_ctx, frames), (unsigned)offsetof(xb_ctx, set_decimals),
             (unsigned)offsetof(xb_ctx, stack_limit), (unsigned)offsetof(xb_array, data),
             (unsigned)offsetof(xb_ref, value), (unsigned)offsetof(xb_hash, entries),
             (unsigned)offsetof(xb_object, vars), (unsigned)offsetof(xb_strbuf, bytes));
    xb_gc_fatal(why);
  }
}
