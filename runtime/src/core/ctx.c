#include "xbrt_int.h"

#include "core/shared.h"
#include "core/shared_file.h"
#include "rdd/rdd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Settings of `from`'s values (NULL: Clipper's: SET EPOCH 1900, the rest off, no codepage or driver chosen) with the
 * date format `fmt` (`len` bytes), used by one environment. */
static xb_settings *settings_new(const xb_settings *from, const char *fmt, size_t len) {
  xb_settings *s = malloc(sizeof *s + len + 1);
  if (!s) xb_gc_fatal("out of memory");
  if (from) {
    memcpy(s, from, sizeof *s);
  } else {
    memset(s, 0, sizeof *s);
    s->epoch = 1900;
  }
  s->refs = 1;
  memcpy(s->date_format, fmt, len); /* `fmt` may be `from`'s own: copied before `from` may go */
  s->date_format[len] = '\0';
  return s;
}

static void settings_release(xb_settings *s) {
  if (s && xb_atomic_add32(&s->refs, (uint32_t)-1, XB_ACQ_REL) == 1) free(s);
}

xb_settings *xb_settings_own(xb_ctx *ctx) {
  xb_env *env = ctx->env;
  xb_settings *s = env->settings;
  if (!env->borrowsSettings && xb_atomic_load32(&s->refs, XB_ACQUIRE) == 1) return s;
  xb_settings *mine = settings_new(s, s->date_format, strlen(s->date_format));
  env->settings = mine;
  if (env->borrowsSettings) env->borrowsSettings = 0;
  else settings_release(s);
  return mine;
}

/* The environment a program starts with: Clipper's SET values (SET DECIMALS is the thread context's), no ErrorBlock()
 * until ErrorSys() sets one (xb_program_run), work area 1 current. */
/* The thread's spare environment, or a new one (zeroed): its fields are the caller's to set. */
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

/* A new environment; `fresh`: with Clipper's settings, else none yet (the caller's to set). */
static xb_env *env_new(xb_ctx *ctx, int fresh) {
  xb_env *env = env_take(ctx);
  memset(env, 0, sizeof *env);
  xb_set_nil(&env->error_block);
  xb_set_nil(&env->language);
  if (fresh) env->settings = settings_new(NULL, "mm/dd/yy", 8);
  env->areaCurrent = 1;
  return env;
}

static void unlist(xb_env *env);

static void env_free(xb_ctx *ctx, xb_env *env) {
  xb_memvar_bindings_free(env);
  for (uint32_t i = 0; i < env->privateLen; ++i) /* an environment that ends with PRIVATEs alive: a task abandoned */
    if (env->privates[i].id != XB_PRIVATE_CLEARED) xb_memvar_unhold(env->privates[i].id);
  env->privateLen = 0;
  if (env->privates) free(env->privates); /* most environments never had these */
  if (env->areas) free(env->areas);
  env->privates = NULL;
  env->areas = NULL;
  if (!env->borrowsSettings) settings_release(env->settings);
  env->settings = NULL;
  env->borrowsSettings = 0;
  unlist(env);
  /* the last this thread listed (a function that suspended and ended on the same thread): spare again */
  if (env->recent && ctx && ctx->recentLen && ctx->recent[ctx->recentLen - 1] == env) {
    --ctx->recentLen;
    env->recent = 0;
  }
  if (env->recent) { /* a thread's recent list holds it: the collector frees it */
    env->ended = 1;
    return;
  }
  if (ctx && !ctx->spareEnv) {
    ctx->spareEnv = env;
    return;
  }
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
  const xb_settings *st = env->settings;
  xb_array *a = xb_array_alloc(SNAP_COUNT_); /* `a`, on the stack, stays where it is */
  xb_set_array(r, a);
  env_sets sets = {ctx->set_decimals, st->epoch,       st->set_exact,    st->set_fixed, st->century,
                   st->set_deleted,  st->set_shared,  st->set_softseek, st->set_unique, st->codepage,
                   st->rddDefault};
  xb_set_str(&xb_array_items(a)[SNAP_SETS], xb_str_new((const char *)&sets, sizeof sets));
  xb_gc_write(a);
  st = ctx->env->settings; /* an allocation may have collected: the settings stay (no collection frees them) */
  xb_set_str(&xb_array_items(a)[SNAP_DATE_FORMAT], xb_str_new(st->date_format, strlen(st->date_format)));
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

/* The snapshot's settings, new: the environment's own. */
static void restore_settings(xb_ctx *ctx, xb_env *env, const xb_item *snapshot) {
  xb_item it;
  env_sets sets;
  xb_array_get(snapshot->v.p, SNAP_SETS, &it);
  memcpy(&sets, xb_str_data(it.v.s), sizeof sets);
  ctx->set_decimals = sets.decimals;
  xb_array_get(snapshot->v.p, SNAP_DATE_FORMAT, &it);
  xb_settings *old = env->settings;
  xb_settings *st = settings_new(old, xb_str_data(it.v.s), it.v.s->len);
  if (old && !env->borrowsSettings) settings_release(old);
  env->settings = st;
  env->borrowsSettings = 0;
  st->epoch = sets.epoch;
  st->set_exact = sets.exact;
  st->set_fixed = sets.fixed;
  st->century = sets.century;
  st->set_deleted = sets.deleted;
  st->set_shared = sets.shared;
  st->set_softseek = sets.softseek;
  st->set_unique = sets.unique;
  st->codepage = sets.codepage;
  st->rddDefault = sets.rddDefault;
}

void xb_env_restore(xb_ctx *ctx, const xb_item *snapshot) {
  xb_env *env = ctx->env;
  xb_item it;
  restore_settings(ctx, env, snapshot);
  xb_array_get(snapshot->v.p, SNAP_ERROR_BLOCK, &env->error_block);
  xb_array_get(snapshot->v.p, SNAP_LANGUAGE, &env->language);
  xb_array_get(snapshot->v.p, SNAP_MEMVARS, &it);
  xb_memvar_restore(ctx, &it);
}

void xb_env_enter(xb_ctx *ctx, const xb_item *snapshot) {
  xb_env *env = env_new(ctx, 0); /* the settings: the snapshot's (xb_env_restore) */
  env->outer = ctx->env;
  env->outerDecimals = ctx->set_decimals;
  ctx->env = env;
  xb_env_restore(ctx, snapshot);
}

xb_env *xb_env_inherit(xb_ctx *ctx) {
  xb_env *from = ctx->env, *env = env_take(ctx);
  *env = *from; /* the settings (borrowed), ErrorBlock(), the language; the rest below */
  env->borrowsSettings = 1;
  env->borrowsBindings = from->bindings != NULL;
  env->privates = NULL;
  env->privateLen = env->privateCap = 0;
  env->file_error = 0;
  env->areas = NULL;
  env->areaCap = 0;
  env->areaCurrent = 1;
  env->netErr = 0;
  env->parkedTask = NULL;
  env->parkedPrev = env->parkedNext = NULL;
  env->listed = env->recent = env->parked = env->ended = 0;
  env->parkedScans = 0;
  env->outer = from;
  env->outerDecimals = ctx->set_decimals;
  ctx->env = env;
  return env;
}

/* The environments of suspended asynchronous functions, in PARK_SHARDS shards by the environment's address (a list
 * and a lock each): an environment joins its shard's list when its function first suspends, and leaves it when the
 * function ends (or its task dies), so a suspension and a resumption take no lock. The collector scans what a parked
 * environment holds: every one at a full collection; at a young one, those parked lately, which each thread lists at
 * a suspension without a lock (`ctx->recent`, an environment once until the collector takes it off; the list of a
 * thread that ended goes to `orphans`), scanned by the next PARK_SCANS_YOUNG young collections (all it reaches is old
 * by then, and a parked environment does not change). An environment that ends while some thread's list holds it is
 * left for the collector to free (`ended`). Those whose tasks died wait in `dead` (under deadLock; `deaths` tells a
 * thread there are some without the lock) until they are closed. */
#define PARK_SHARDS 64u
#define PARK_SCANS_YOUNG 2u
typedef struct park_shard {
  uint32_t lock; /* xb_word_lock: taken when a function first suspends and when it ends */
  xb_env *list;
} park_shard;
static park_shard shards[PARK_SHARDS];
static xb_mutex deadLock; /* `dead`, and `orphans` */
static xb_once_flag parkedMade = XB_ONCE_INIT;
static xb_env *dead;
static uint32_t deaths;
static xb_env **orphans;
static uint32_t orphanLen, orphanCap;
static uint32_t parkedReady; /* the locks made: a collection before any park has none to look at */

static void make_parked(void) {
  if (!xb_mutex_init(&deadLock)) xb_gc_fatal("cannot create the environments' lock");
  xb_atomic_store32(&parkedReady, 1, XB_RELEASE);
}

static void unlink_parked(xb_env *env) {
  if (env->parkedPrev) env->parkedPrev->parkedNext = env->parkedNext;
  else shards[env->parkedShard].list = env->parkedNext;
  if (env->parkedNext) env->parkedNext->parkedPrev = env->parkedPrev;
  env->parkedPrev = env->parkedNext = NULL;
}

static void link_parked(xb_env **list, xb_env *env) {
  env->parkedPrev = NULL;
  env->parkedNext = *list;
  if (*list) (*list)->parkedPrev = env;
  *list = env;
}

/* Appends `env` to a list of environments (a thread's `recent`, the orphans). */
static void push_env(xb_env ***list, uint32_t *len, uint32_t *cap, xb_env *env) {
  if (*len == *cap) {
    uint32_t n = *cap ? *cap * 2 : 64;
    xb_env **grown = realloc(*list, n * sizeof *grown);
    if (!grown) xb_gc_fatal("out of memory");
    *list = grown;
    *cap = n;
  }
  (*list)[(*len)++] = env;
}

void xb_env_park(xb_ctx *ctx, xb_env *env, void *task) {
  if (env->borrowsSettings) { /* its caller goes on: the settings and bindings it used are its own now too */
    xb_atomic_add32(&env->settings->refs, 1, XB_RELAXED);
    env->borrowsSettings = 0;
  }
  if (env->borrowsBindings) {
    xb_memvar_bindings_take(env);
    env->borrowsBindings = 0;
  }
  env->parkedDecimals = ctx->set_decimals;
  ctx->env = env->outer;
  ctx->set_decimals = env->outerDecimals;
  env->outer = NULL;
  env->parkedTask = task;
  env->parked = 1;
  env->parkedScans = 0;
  if (!env->listed) { /* its first suspension */
    xb_once(&parkedMade, make_parked);
    env->parkedShard = (uint8_t)(((uintptr_t)env >> 6) % PARK_SHARDS);
    park_shard *s = &shards[env->parkedShard];
    xb_word_lock(&s->lock); /* no safepoint until it is released: no collection meanwhile */
    link_parked(&s->list, env);
    xb_word_unlock(&s->lock);
    env->listed = 1;
  }
  if (!env->recent) {
    push_env(&ctx->recent, &ctx->recentLen, &ctx->recentCap, env);
    env->recent = 1;
  }
}

void xb_env_unpark(xb_ctx *ctx, xb_env *env) {
  env->parked = 0;
  env->outer = ctx->env;
  env->outerDecimals = ctx->set_decimals;
  ctx->env = env;
  ctx->set_decimals = env->parkedDecimals;
}

/* Out of its shard's list, if it is in it: its function ended. */
static void unlist(xb_env *env) {
  if (!env->listed) return;
  park_shard *s = &shards[env->parkedShard];
  xb_word_lock(&s->lock);
  unlink_parked(env);
  xb_word_unlock(&s->lock);
  env->listed = 0;
}

void xb_env_recent_orphan(xb_ctx *ctx) {
  if (ctx->recentLen) {
    xb_once(&parkedMade, make_parked);
    xb_mutex_lock(&deadLock);
    for (uint32_t i = 0; i < ctx->recentLen; ++i) push_env(&orphans, &orphanLen, &orphanCap, ctx->recent[i]);
    xb_mutex_unlock(&deadLock);
  }
  free(ctx->recent); /* a collection may still run before the context leaves the collector's list: none left */
  ctx->recent = NULL;
  ctx->recentLen = ctx->recentCap = 0;
}

void xb_env_parked_each(xb_ctx *contexts, void (*fn)(xb_env *env), int full) {
  if (!xb_atomic_load32(&parkedReady, XB_ACQUIRE)) return; /* none parked yet */
  if (full)
    for (uint32_t k = 0; k < PARK_SHARDS; ++k)
      for (xb_env *env = shards[k].list; env; env = env->parkedNext)
        if (env->parked) fn(env); /* one running is its context's */
  for (xb_ctx *ctx = contexts; ctx; ctx = ctx->gcNext)
    for (uint32_t i = 0; i < ctx->recentLen; ++i)
      if (!ctx->recent[i]->ended && ctx->recent[i]->parked) fn(ctx->recent[i]);
  for (uint32_t i = 0; i < orphanLen; ++i)
    if (!orphans[i]->ended && orphans[i]->parked) fn(orphans[i]);
  for (xb_env *env = dead; env; env = env->parkedNext) fn(env);
}

/* A listed environment after a collection's marking: its task's address now; to `dead` when that died (1). */
static int settle_one(xb_env *env, void *(*live)(void *obj)) {
  env->parkedTask = live(env->parkedTask);
  if (env->parkedTask) return 0;
  unlink_parked(env); /* every other thread stopped: no lock needed, none held at a safepoint */
  env->listed = 0;
  env->parkedNext = dead;
  if (dead) dead->parkedPrev = env;
  dead = env;
  return 1;
}

/* A thread's recent list after a collection's marking: the environments that ended freed, those scanned enough (or
 * dead) taken off; the rest kept for the next young collection. */
static int settle_recent(xb_env **list, uint32_t *len, void *(*live)(void *obj), int full) {
  int died = 0;
  uint32_t kept = 0;
  for (uint32_t i = 0; i < *len; ++i) {
    xb_env *env = list[i];
    if (env->ended) {
      free(env);
      continue;
    }
    if (!full && env->listed && env->parked) died |= settle_one(env, live);
    if (env->listed && env->parked && ++env->parkedScans < PARK_SCANS_YOUNG) list[kept++] = env;
    else env->recent = 0;
  }
  *len = kept;
  return died;
}

int xb_env_parked_settle(xb_ctx *contexts, void *(*live)(void *obj), int full) {
  if (!xb_atomic_load32(&parkedReady, XB_ACQUIRE)) return 0;
  int died = 0;
  if (full)
    for (uint32_t k = 0; k < PARK_SHARDS; ++k)
      for (xb_env *env = shards[k].list, *next; env; env = next) {
        next = env->parkedNext;
        if (env->parked) died |= settle_one(env, live);
        else env->parkedTask = live(env->parkedTask); /* running: its task is reachable */
      }
  for (xb_ctx *ctx = contexts; ctx; ctx = ctx->gcNext) died |= settle_recent(ctx->recent, &ctx->recentLen, live, full);
  died |= settle_recent(orphans, &orphanLen, live, full);
  if (died) xb_atomic_store32(&deaths, 1, XB_RELEASE);
  return died;
}

/* Closes the environments whose tasks died, one at a time `ctx`'s (its work areas closed as a flow's at its end);
 * `all`: every parked one too (the program's end). */
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
    while (!env && all && k < PARK_SHARDS) {
      park_shard *s = &shards[k];
      xb_word_lock(&s->lock);
      for (env = s->list; env && !env->parked; env = env->parkedNext) {
      }
      if (env) unlink_parked(env);
      xb_word_unlock(&s->lock);
      if (env) env->listed = 0;
      else ++k;
    }
    if (!env) return;
    env->parked = 0;
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
  ctx->env = env_new(NULL, 1);
  ctx->set_decimals = 2;
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
  xb_env_recent_orphan(ctx); /* before the collector stops seeing the context */
  xb_gc_unregister(ctx);
  xb_memvar_cache_drop(ctx);
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
 * that recurses a little deeper) pays the system's call and the pages' faults that often at most, staying deep
 * between. */
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
  xb_activation a;
  xb_activation_first(&a, ctx->frames);
  const char *name = a.frame && a.proc ? a.proc->name : "";
  xb_error_described(ctx, NULL, EG_LIMIT, 0, "Stack overflow", name, 0, 0, NULL);
  ctx->stack_limit = limit;
}

static int32_t exitCode;

int32_t xb_exit_code(void) { return (int32_t)xb_atomic_load32((uint32_t *)&exitCode, XB_RELAXED); }
void xb_exit_code_set(int32_t code) { xb_atomic_store32((uint32_t *)&exitCode, (uint32_t)code, XB_RELAXED); }

void xb_ctx_set_date_format(xb_ctx *ctx, const char *s, size_t len) {
  xb_settings *old = ctx->env->settings;
  ctx->env->settings = settings_new(old, s, len); /* `s` may be the old picture: copied first */
  if (ctx->env->borrowsSettings) ctx->env->borrowsSettings = 0;
  else settings_release(old);
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

void xb_abi_check(uint32_t version, uint32_t item_size, uint32_t str_hdr_size, uint32_t block_params_offset,
                  uint32_t block_cells_offset, uint32_t frame_slots_offset, uint32_t ctx_frames_offset,
                  uint32_t ctx_decimals_offset, uint32_t ctx_stack_limit_offset, uint32_t ctx_lent_offset,
                  uint32_t ctx_tlab_offset, uint32_t array_data_offset, uint32_t array_items_offset,
                  uint32_t ref_value_offset, uint32_t hash_entries_offset, uint32_t object_vars_offset,
                  uint32_t strbuf_bytes_offset) {
  if (version != XB_ABI_VERSION || item_size != sizeof(xb_item) || str_hdr_size != offsetof(xb_str, bytes) ||
      block_params_offset != offsetof(xb_block, nparams) || block_cells_offset != offsetof(xb_block, cells) ||
      frame_slots_offset != offsetof(xb_frame, slots) ||
      ctx_frames_offset != offsetof(xb_ctx, frames) || ctx_decimals_offset != offsetof(xb_ctx, set_decimals) ||
      ctx_stack_limit_offset != offsetof(xb_ctx, stack_limit) || ctx_lent_offset != offsetof(xb_ctx, lent) ||
      ctx_tlab_offset != offsetof(xb_ctx, tlab) || array_data_offset != offsetof(xb_array, data) ||
      array_items_offset != offsetof(xb_array, inline_items) ||
      ref_value_offset != offsetof(xb_ref, value) ||
      hash_entries_offset != offsetof(xb_hash, entries) || object_vars_offset != offsetof(xb_object, vars) ||
      strbuf_bytes_offset != offsetof(xb_strbuf, bytes)) {
    char why[640];
    snprintf(why, sizeof why,
             "ABI mismatch (module v%u item=%u str=%u block=%u/%u frame=%u ctx=%u/%u/%u/%u/%u array=%u/%u ref=%u "
             "hash=%u object=%u strbuf=%u, runtime v%u item=%u str=%u block=%u/%u frame=%u ctx=%u/%u/%u/%u/%u "
             "array=%u/%u ref=%u hash=%u object=%u strbuf=%u)",
             version, item_size, str_hdr_size, block_params_offset, block_cells_offset, frame_slots_offset,
             ctx_frames_offset,
             ctx_decimals_offset, ctx_stack_limit_offset, ctx_lent_offset, ctx_tlab_offset, array_data_offset,
             array_items_offset, ref_value_offset, hash_entries_offset, object_vars_offset, strbuf_bytes_offset,
             XB_ABI_VERSION, (unsigned)sizeof(xb_item), (unsigned)offsetof(xb_str, bytes),
             (unsigned)offsetof(xb_block, nparams), (unsigned)offsetof(xb_block, cells),
             (unsigned)offsetof(xb_frame, slots),
             (unsigned)offsetof(xb_ctx, frames), (unsigned)offsetof(xb_ctx, set_decimals),
             (unsigned)offsetof(xb_ctx, stack_limit), (unsigned)offsetof(xb_ctx, lent),
             (unsigned)offsetof(xb_ctx, tlab), (unsigned)offsetof(xb_array, data),
             (unsigned)offsetof(xb_array, inline_items),
             (unsigned)offsetof(xb_ref, value), (unsigned)offsetof(xb_hash, entries),
             (unsigned)offsetof(xb_object, vars), (unsigned)offsetof(xb_strbuf, bytes));
    xb_gc_fatal(why);
  }
}
