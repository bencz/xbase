/* Thread contexts and the environments of flows. */
#ifndef XB_CONTEXT_H
#define XB_CONTEXT_H

#include "gc/gc.h"
#include "platform/atomic.h"

#define XB_MACRO_CACHE 64

/* The environment of a flow of execution: what a program sets and sees as its state (the SET values, ErrorBlock(),
 * memory variables, work areas), apart from the thread that runs it. A thread context points at the environment of
 * the flow it runs (docs/concurrency.md). */
typedef struct xb_env {
  /* ErrorBlock(): a codeblock, NIL until ErrorSys() runs (a root of the collector) */
  xb_item  error_block;
  /* Memory variables (core/memvar.c): by number, the binding visible (an
   * XB_T_CELL item, NIL for none); the PRIVATEs alive, each with the binding
   * it hid, in creation order. Roots of the collector. */
  xb_item  *memvars;
  uint32_t  memvarCap;
  struct xb_private *privates;
  uint32_t  privateLen, privateCap;
  int      set_exact;
  int      set_fixed;       /* SET FIXED: numbers shown with SET DECIMALS decimals (core/numeric.h) */
  /* SET DATE FORMAT picture (NUL-terminated): in `dateOwn` (malloc'ed, dateCap bytes), or borrowed from the
   * environment that is set aside while this one runs (xb_env_inherit), until it changes or this one is parked. */
  const char *date_format;
  char    *dateOwn;
  size_t   dateCap;
  int32_t  epoch;           /* SET EPOCH */
  int      century;         /* SET CENTURY */
  int32_t  file_error;      /* FError(): the code of the last file function (rtl/files.c) */
  xb_item  language;        /* SET LANGUAGE: the current language's texts (core/language.c), a root; NIL until used */
  const struct xb_codepage *codepage; /* SET CODEPAGE (core/codepage.h); NULL: UTF-8 */
  /* Work areas (rdd/rdd.h): by number, NULL where no table is open; the current one's number (1 at the start); SET
   * DELETED; the default driver (NULL: DBFNTX); NetErr(). */
  struct xb_area **areas;
  uint32_t areaCap, areaCurrent;
  int      set_deleted;
  int      set_shared;     /* SET EXCLUSIVE OFF: USE opens shared unless told EXCLUSIVE (zero: on, Clipper's default) */
  int      set_softseek;   /* SET SOFTSEEK: a SEEK that finds nothing stops on the next key */
  int      set_unique;     /* SET UNIQUE: orders made keep one entry per key */
  const struct xb_rdd *rddDefault;
  int      netErr;
  struct xb_env *outer;    /* the environment set aside while this one runs (xb_env_enter), NULL: none; a root too */
  int32_t  outerDecimals;  /* SET DECIMALS of the code that set it aside, back when this one ends */
  /* While an asynchronous function that owns it is suspended (xb_env_park): its SET DECIMALS, the task (its address,
   * which the collector updates; the environment goes when the task does), and its neighbors in the parked list. */
  int32_t  parkedDecimals;
  void    *parkedTask;
  struct xb_env *parkedPrev, *parkedNext;
} xb_env;

/* A thread's context: what compiled code reaches (the [ABI] head), the state of the code running on the thread now
 * (its frames, C roots, a BREAK on its way, the error blocks running), caches of the thread's own, and the
 * environment of the flow it runs. */
struct xb_ctx {
  uint32_t  pending;        /* [ABI] the first four fields, in this order (see xbrt.h) */
  int32_t   set_decimals;   /* SET DECIMALS: compiled code reads it for / and % (the thread's until tasks switch) */
  xb_frame *frames;         /* innermost frame of compiled code; frames are GC roots */
  const char *stack_limit;  /* compiled code raises the stack error below it (core/ctx.c) */
  xb_env   *env;
  const char *stack_low;    /* the lowest address of the thread's stack the program may use (NULL: unknown) */
  xb_ctx  *gcNext, *gcPrev; /* registered root sets */
  struct xb_tlh *gcThread;  /* the attached thread that registered it (gc.c), NULL: none */
  xb_item **roots;          /* temporary C roots (xb_gc_root_push) */
  unsigned rootLen, rootCap;
  xb_item  break_value;     /* the value of a pending BREAK (a root of the collector) */
  uint32_t error_depth;     /* error blocks running */
  uint32_t task_depth;      /* tasks running on the thread (one inside another that waits for it): __TaskFail() */
  /* The step of an asynchronous function that runs on the thread (rtl/tasks.c, on the C stack): its resume function
   * finds its arguments, its frame and its task there. */
  struct xb_async_step *async_step;
  /* AWAIT of a call to an asynchronous function (xb_await_fuse): the function's entry (or, `async_fuse_send`, the
   * method the send reaches: xb_fuse_target), and the frame that calls it; `async_done` once its first step gave the
   * result itself (it did not suspend: no task made). */
  xb_func async_fuse;
  const xb_frame *async_fuse_frame;
  int32_t async_fuse_send;
  int32_t async_done;
  xb_env  *spareEnv;        /* an environment ended, kept for the next one the thread makes (no allocation) */
  /* Type() evaluating (error_trap > 0): an error runs no block, it ends the
   * evaluation (XB_PENDING_TRAP, which no BEGIN SEQUENCE catches) and sets
   * error_trapped to its generic code (never 0). */
  uint32_t error_trap;
  int32_t  error_trapped;
  /* Macros compiled lately (macro/run.c): per entry, chosen by a hash of the
   * text, the text and its compiled function (roots of the collector), and
   * what the text was compiled as (mgoal). The compiled function depends on
   * the text alone, so the cache is the thread's. */
  xb_item  macro_cache[2 * XB_MACRO_CACHE];
  uint8_t  macro_cache_goal[XB_MACRO_CACHE];
};

/* A send that an AWAIT fused (xb_await_fuse) reaches the method `fn` (NULL: a runtime class's): a compiled method run
 * right from the frame of that AWAIT becomes the entry it asks for; any other ends the request. */
static inline void xb_fuse_target(xb_ctx *ctx, xb_func fn) {
  if (!ctx->async_fuse_send) return;
  ctx->async_fuse_send = 0;
  if (fn && ctx->async_fuse_frame == ctx->frames) ctx->async_fuse = fn;
}

/* ctx->pending: other threads set requests there (a collection's safepoint, objects to finalize, the program's QUIT),
 * so every change is an atomic read-modify-write, and code that sets aside or ends requests touches only the flow's
 * own (BREAK, QUIT, TRAP: XB_PENDING_UNWIND), never another thread's; code that sets requests aside clears those it
 * read, so a QUIT raised meanwhile stays. A request is set with release and read with acquire: whoever
 * sees it sees what was written before it (the collector's `stopping`). */
static inline uint32_t xb_pending_get(const xb_ctx *ctx) { return xb_atomic_load32(&ctx->pending, XB_ACQUIRE); }
static inline void xb_pending_set(xb_ctx *ctx, uint32_t bits) { xb_atomic_or32(&ctx->pending, bits, XB_RELEASE); }
static inline void xb_pending_clear(xb_ctx *ctx, uint32_t bits) { xb_atomic_and32(&ctx->pending, ~bits, XB_RELAXED); }

/* The C roots (xb_gc_root_push and xb_gc_root_pop, xbrt.h) inline for the runtime's own code, which reaches them
 * under those names; the vector grows in xb_gc_roots_grow. The exported functions stay for compiled code. */
void xb_gc_roots_grow(xb_ctx *ctx);
static inline void xb_root_push(xb_ctx *ctx, xb_item *it) {
  if (ctx->rootLen == ctx->rootCap) xb_gc_roots_grow(ctx);
  ctx->roots[ctx->rootLen++] = it;
}
static inline void xb_root_pop(xb_ctx *ctx, unsigned n) { ctx->rootLen -= n; }
#ifndef XB_GC_ROOTS_DEFINED
#define xb_gc_root_push xb_root_push
#define xb_gc_root_pop xb_root_pop
#endif

void xb_ctx_set_date_format(xb_ctx *ctx, const char *s, size_t len);

/* The lowest address of the stack of the context's thread that the program may use: compiled code raises the stack
 * error a reserve above it (core/ctx.c); NULL: unknown, no limit. */
void xb_ctx_set_stack(xb_ctx *ctx, const char *low);

/* A new flow's environment (a thread's, a task's). xb_env_capture makes in `*r` (rooted) a snapshot of `ctx`'s, an
 * array the new flow keeps: the SET values (SET DECIMALS too), the date format, ErrorBlock(), the language's texts,
 * the code page, the default driver and the memory variables visible (the same variables); its work areas are not in
 * it. xb_env_restore gives `ctx`'s environment, new and empty, the values of a snapshot. xb_env_enter sets `ctx`'s
 * environment aside for a new one restored from a snapshot (a task that runs on the thread), xb_env_leave closes the
 * new one's work areas, frees it and puts the one set aside back (SET DECIMALS too). */
void xb_env_capture(xb_ctx *ctx, xb_item *r);
void xb_env_restore(xb_ctx *ctx, const xb_item *snapshot);
void xb_env_enter(xb_ctx *ctx, const xb_item *snapshot);
void xb_env_leave(xb_ctx *ctx);

/* The environment of an asynchronous function (rtl/tasks.c), its own from its call to its end, across its steps
 * (docs/concurrency.md#asynchronous-functions). xb_env_inherit sets `ctx`'s aside for a new one with its values (the
 * SET values, ErrorBlock(), the language, the memory variables visible: the same variables) and no work area open, and
 * returns it; xb_env_leave ends it. Between two steps it is parked: xb_env_park sets it aside (SET DECIMALS kept), a
 * root of the collector until xb_env_unpark puts it back as `ctx`'s for the next step. A parked environment whose task
 * the collector finds unreachable (xb_env_parked_settle) is closed later by xb_env_reap (its work areas, then the
 * rest), which any thread may call between flows; xb_env_reap_all closes every parked one (the program's end). */
xb_env *xb_env_inherit(xb_ctx *ctx);
void xb_env_park(xb_ctx *ctx, xb_env *env, void *task);
void xb_env_unpark(xb_ctx *ctx, xb_env *env);
void xb_env_reap(xb_ctx *ctx);
void xb_env_reap_all(xb_ctx *ctx);
/* The collector's side: `fn` on each parked environment (marking, every other thread stopped); then `live` gives
 * where each one's task is now, NULL when it is unreachable: those are the environments to reap. */
void xb_env_parked_each(void (*fn)(xb_env *env));
void xb_env_parked_settle(void *(*live)(void *obj));

/* Closes the work areas of `ctx`'s environment whatever ended its flow: the requests pending (and a BREAK's value) set
 * aside meanwhile and back after; what closing raised dropped, but a QUIT (also one another thread raised). */
void xb_env_close_areas(xb_ctx *ctx);

/* The program's threads (rtl/threads.c). xb_program_thread_start runs body( ctx, arg ) on a new thread, attached to
 * the collector, with a context of its own (QUIT pending when the program is ending): 0 when the system could not
 * make one. xb_quit_all ends the program from any thread: every other context is asked to QUIT, and the scheduler's
 * workers to stop. xb_threads_end is the program's end: the threads that still run (ThreadStart()'s, the workers)
 * are asked to QUIT and waited for, and the tasks that did not start are canceled: before the EXIT procedures run,
 * and after them. */
int xb_program_thread_start(void (*body)(xb_ctx *ctx, void *arg), void *arg);
void xb_quit_all(const xb_ctx *ctx);
void xb_threads_end(xb_ctx *ctx);

/* Eval( block, args... ) of an array that holds the block (or function reference) and its arguments: a thread's
 * work, a task's. */
void xb_eval_work(xb_ctx *ctx, xb_item *r, const xb_item *work);

/* The program's exit code (ErrorLevel(), an unrecoverable error): the process's, whatever thread sets it. */
int32_t xb_exit_code(void);
void xb_exit_code_set(int32_t code);

/* core/language.c: selects a language by name (any letter case): 0 when the runtime has none of that name. The
 * current language's texts (English when none was selected), NULL only when they cannot be made; an error's
 * description in it, NULL when it has none. */
int xb_language_select(xb_ctx *ctx, const char *id, size_t len);
const xb_array *xb_language(xb_ctx *ctx);
xb_str *xb_language_error(xb_ctx *ctx, int32_t gencode, int32_t subcode);

typedef struct xb_private {
  uint32_t id;              /* the variable's number (none once CLEAR MEMORY released it, core/memvar.c) */
  uint32_t borrowed;        /* its binding is an argument passed by reference (PARAMETERS) */
  const xb_frame *owner;    /* the frame of the activation that created it */
  xb_item  hidden;          /* the binding it hid (NIL: none) */
} xb_private;

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

/* The xb_proc of a runtime function that shows on the call stack: its name,
 * no parameters in its frame, `flags`, no source file. */
#define XB_RUNTIME_PROC(name, flags) {name, 0, flags, NULL}

/* Whether the call stack (ProcName(), the "Called from" lines) shows a frame. */
static inline int xb_frame_shown(const xb_frame *f) { return f->proc && !(f->proc->flags & XB_PROC_HIDDEN); }

/* SET DECIMALS as an item's decimal count (items keep 16 bits, as in Clipper). */
static inline uint16_t xb_decimals(const xb_ctx *ctx) { return (uint16_t)ctx->set_decimals; }

#endif
