/* Thread contexts and the environments of flows. */
#ifndef XB_CONTEXT_H
#define XB_CONTEXT_H

#include "gc/gc.h"
#include "platform/atomic.h"

#define XB_MACRO_CACHE 64
#define XB_NAME_CACHE 32 /* names a context keeps held (core/memvar.c, xb_memvar_use) */

/* The environment of a flow of execution: what a program sets and sees as its state (the SET values, ErrorBlock(),
 * memory variables, work areas), apart from the thread that runs it. A thread context points at the environment of
 * the flow it runs (docs/concurrency.md). */
typedef struct xb_bindings xb_bindings;

/* The SET values a flow inherits as they are when it starts (an ASYNC function's first step, a task's): shared by the
 * environments that inherited them, read in place by all, and never changed while shared: an environment that changes
 * one takes a copy of its own first (xb_settings_own), so a function suspended at a thousand places costs its caller's
 * settings once. */
typedef struct xb_settings {
  uint32_t refs;            /* the environments using it (atomic: they may end on other threads) */
  int      set_exact;
  int      set_fixed;       /* SET FIXED: numbers shown with SET DECIMALS decimals (core/numeric.h) */
  int32_t  epoch;           /* SET EPOCH */
  int      century;         /* SET CENTURY */
  const struct xb_codepage *codepage; /* SET CODEPAGE (core/codepage.h); NULL: UTF-8 */
  int      set_deleted;
  int      set_shared;     /* SET EXCLUSIVE OFF: USE opens shared unless told EXCLUSIVE (zero: on, Clipper's default) */
  int      set_softseek;   /* SET SOFTSEEK: a SEEK that finds nothing stops on the next key */
  int      set_unique;     /* SET UNIQUE: orders made keep one entry per key */
  const struct xb_rdd *rddDefault; /* the default driver (NULL: DBFNTX) */
  char     date_format[];   /* SET DATE FORMAT picture, NUL-terminated */
} xb_settings;

typedef struct xb_env {
  /* ErrorBlock(): a codeblock, NIL until ErrorSys() runs (a root of the collector) */
  xb_item  error_block;
  /* Memory variables (core/memvar.c): by number, the bindings visible (XB_T_CELL items; NULL: none), shared with the
   * environments that inherit them until one changes them; the PRIVATEs alive, each with the binding it hid, in
   * creation order. Roots of the collector. */
  xb_bindings *bindings;
  uint32_t  bindHint;       /* the entry of the bindings found last: tried first */
  struct xb_private *privates;
  uint32_t  privateLen, privateCap;
  xb_settings *settings;    /* the SET values (shared: read them, change them through xb_settings_own) */
  int32_t  file_error;      /* FError(): the code of the last file function (rtl/files.c) */
  /* SET LANGUAGE: the current language's texts (core/language.c), a root; NIL until used. A flow's own, as ErrorBlock()
   * is (BEGIN SEQUENCE WITH sets it each time): neither would take a copy of the settings. */
  xb_item  language;
  /* Work areas (rdd/rdd.h): by number, NULL where no table is open; the current one's number (1 at the start);
   * NetErr(). */
  struct xb_area **areas;
  uint32_t areaCap, areaCurrent;
  int      netErr;
  struct xb_env *outer;    /* the environment set aside while this one runs (xb_env_enter), NULL: none; a root too */
  int32_t  outerDecimals;  /* SET DECIMALS of the code that set it aside, back when this one ends */
  /* An asynchronous function's (xb_env_park, core/ctx.c): its SET DECIMALS while suspended, its task (its address,
   * which the collector updates; the environment goes when the task does), its neighbors in its shard's list. */
  int32_t  parkedDecimals;
  void    *parkedTask;
  struct xb_env *parkedPrev, *parkedNext;
  uint8_t  parkedShard;     /* its list's shard */
  uint8_t  parkedScans;     /* young collections that scanned it since it was last parked */
  uint8_t  listed;          /* in its shard's list: from its first suspension to its function's end */
  uint8_t  parked;          /* suspended now (else running: its context's) */
  uint8_t  recent;          /* in a thread's recent list (or the orphans): young collections scan it */
  uint8_t  ended;           /* its function ended while a recent list held it: the collector frees it */
  /* An asynchronous function's environment borrows its caller's settings and bindings (xb_env_inherit) without a
   * reference while it runs on the caller's thread, the caller waiting for it: it takes one when it first parks (the
   * caller then goes on and may change them in place), and copies them when it changes them (xb_settings_own,
   * bindings_own) before either. */
  uint8_t  borrowsSettings, borrowsBindings;
} xb_env;

/* A thread's context: what compiled code reaches (the [ABI] head), the state of the code running on the thread now
 * (its frames, C roots, a BREAK on its way, the error blocks running), caches of the thread's own, and the
 * environment of the flow it runs. */
struct xb_ctx {
  uint32_t  pending;        /* [ABI] the first five fields, in this order (see xbrt.h) */
  int32_t   set_decimals;   /* SET DECIMALS: compiled code reads it for / and % (the thread's until tasks switch) */
  xb_frame *frames;         /* innermost frame of compiled code; frames are GC roots */
  const char *stack_limit;  /* below it compiled code calls xb_stack_deep, then raises the stack error (core/ctx.c) */
  xb_item  *lent;           /* the arguments a compiled caller lends for its call (xbrt.h) */
  xb_env   *env;
  const char *stack_low;    /* the lowest address of the thread's stack the program may use (NULL: unknown) */
  const char *stack_mark;   /* where a deep stack starts (xb_stack_deep), NULL: no mark on this stack */
  int64_t stack_given;      /* when its pages were last given back (monotonic ns), 0: never */
  xb_ctx  *gcNext, *gcPrev; /* registered root sets */
  struct xb_tlh *gcThread;  /* the attached thread that registered it (gc.c), NULL: none */
  xb_item **roots;          /* temporary C roots (xb_gc_root_push) */
  unsigned rootLen, rootCap;
  xb_item  break_value;     /* the value of a pending BREAK (a root of the collector) */
  uint32_t error_depth;     /* error blocks running */
  uint32_t quitAside;       /* QUITs set aside (ALWAYS code running meanwhile, xb_seq_suspend), and the program's end
                             * in progress on this context (xb_threads_end): its waits end once the program is ending */
  uint32_t task_depth;      /* tasks running on the thread (one inside another that waits for it): __TaskFail() */
  /* The frames when the innermost task's step began (with task_depth): the task's calls are those above them, the
   * same whether the step runs on a worker or inline on the thread of its caller or waiter (ProcName() and its kin). */
  const xb_frame *task_base;
  /* The step of an asynchronous function that runs on the thread (rtl/tasks.c, on the C stack): its resume function
   * finds its arguments, its frame and its task there. */
  struct xb_async_step *async_step;
  /* The word xb_wait_until sleeps on (rtl/threads.c), published before its QUIT test: xb_gc_raise_all wakes it after
   * it raised a QUIT here, so a waiter ends without looking at the clock. NULL when it waits for nothing. */
  const uint32_t *waitingOn;
  /* AWAIT of a call to an asynchronous function (xb_await_fuse): the function's entry (or, `async_fuse_send`, the
   * method the send reaches: xb_fuse_target), and the frame that calls it; `async_done` once its first step gave the
   * result itself (it did not suspend: no task made). */
  xb_func async_fuse;
  const xb_frame *async_fuse_frame;
  int32_t async_fuse_send;
  int32_t async_done;
  /* The method of a runtime class running now is the target of that AWAIT's send (xb_runtime_perform): one that ends
   * at once may answer with its result itself (xb_await_method, xb_task_answer). */
  int32_t async_fuse_method;
  xb_env  *spareEnv;        /* an environment ended, kept for the next one the thread makes (no allocation) */
  xb_env **recent;          /* environments suspended lately on this thread (core/ctx.c): young collections scan them */
  uint32_t recentLen, recentCap;
  /* Type() evaluating (error_trap > 0): an error runs no block, it ends the
   * evaluation (XB_PENDING_TRAP, which no BEGIN SEQUENCE catches) and sets
   * error_trapped to its generic code (never 0). */
  uint32_t error_trap;
  int32_t  error_trapped;
  /* Macros compiled lately (macro/run.c): per entry, in a set of two a hash of the text chooses, the text and its
   * compiled function (roots of the collector), and what the text was compiled as (mgoal); per set, the entry the next
   * text takes. The compiled function depends on the text alone, so the cache is the thread's. */
  xb_item  macro_cache[2 * XB_MACRO_CACHE];
  uint8_t  macro_cache_goal[XB_MACRO_CACHE];
  uint8_t  macro_cache_next[XB_MACRO_CACHE / 2];
  /* Names of memory variables the thread used lately by their text (core/memvar.c, xb_memvar_use): per entry, chosen
   * by a hash of the name, its number (UINT32_MAX: none), which the context holds, and the operations using it now. */
  uint32_t nameIds[XB_NAME_CACHE];
  uint32_t namePins[XB_NAME_CACHE];
};

/* A send that an AWAIT fused (xb_await_fuse) reaches the method `fn` (NULL: a runtime class's): a compiled method run
 * right from the frame of that AWAIT becomes the entry it asks for; any other ends the request. Whether the method
 * is that AWAIT's, called right from its frame. */
static inline int xb_fuse_target(xb_ctx *ctx, xb_func fn) {
  if (!ctx->async_fuse_send) return 0;
  ctx->async_fuse_send = 0;
  int here = ctx->async_fuse_frame == ctx->frames;
  if (fn && here) ctx->async_fuse = fn;
  return here;
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
/* The environment's settings, its own to change: a copy taken first when other environments share them. */
xb_settings *xb_settings_own(xb_ctx *ctx);

/* The stack of the context's thread, from the lowest address the program may use to its end (it grows down from
 * there): compiled code raises the stack error a reserve above `low` (core/ctx.c); NULL: unknown, no limit. The
 * context becomes the thread's for xb_stack_give_back. */
void xb_ctx_set_stack(xb_ctx *ctx, const char *low, const char *high);
/* The pages of a deep stack (one that went past its mark, XB_STACK_KEEP below its end), given back to the system once
 * the calling thread is XB_STACK_GIVE_ROOM above the mark again (at most once in XB_STACK_GIVE_EVERY_NS), and the mark
 * watched again: called where a stack often is (a sequence that recovers, a task's step that ended, a blocking
 * operation, after a collection's safepoint); nothing on a stack that did not go deep. */
void xb_stack_give_back(void);
/* Whether the thread's stack has room left for a task to run inline (XB_STACK_INLINE above the stack error's limit,
 * or bounds unknown): a waiter short of it waits instead, and a worker runs the task on a stack of its own. */
int xb_stack_room(const xb_ctx *ctx);

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
/* The collector's side, every other thread stopped: `fn` on each parked environment a collection must scan (`full`:
 * all of them; else those parked lately); then `live` gives where each one's task is now, NULL when it is unreachable:
 * those are the environments to reap (1 when some are). */
void xb_env_parked_each(xb_ctx *contexts, void (*fn)(xb_env *env), int full);
int  xb_env_parked_settle(xb_ctx *contexts, void *(*live)(void *obj), int full);
/* A context going: its recent list to the orphans, which the collector scans as a thread's. */
void xb_env_recent_orphan(xb_ctx *ctx);

/* Closes the work areas of `ctx`'s environment whatever ended its flow: the requests pending (and a BREAK's value) set
 * aside meanwhile and back after; what closing raised dropped, but a QUIT (also one another thread raised). */
void xb_env_close_areas(xb_ctx *ctx);

/* The program's threads (rtl/threads.c). xb_program_thread_start runs body( ctx, arg ) on a new thread, attached to
 * the collector, with a context of its own (QUIT pending when the program is ending): 0 when the system could not
 * make one. xb_quit_all ends the program from any thread: every other context is asked to QUIT, and the scheduler's
 * workers to stop. xb_threads_end is the program's end: the threads that still run (ThreadStart()'s, the workers)
 * are asked to QUIT and waited for, and the tasks that did not start are canceled: before the EXIT procedures run,
 * and after them. `stack` is the stack the thread may grow to (reserved, committed as it is used):
 * XB_PROGRAM_THREAD_STACK for ThreadStart()'s threads, XB_WORKER_STACK for the scheduler's workers and its timers'
 * thread, of which there may be hundreds (most of it for the program's calls, above the stack error's reserve,
 * XB_STACK_RESERVE in core/ctx.c). */
#define XB_PROGRAM_THREAD_STACK ((size_t)16 << 20)
#define XB_WORKER_STACK ((size_t)4 << 20)
int xb_program_thread_start(void (*body)(xb_ctx *ctx, void *arg), void *arg, size_t stack);
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

/* xb_private.id of a PRIVATE that CLEAR MEMORY released: it no longer names a variable (nor holds a name), and
 * restores nothing when its activation ends. */
#define XB_PRIVATE_CLEARED UINT32_MAX

typedef struct xb_private {
  uint32_t id;              /* the variable's number, which it holds (XB_PRIVATE_CLEARED: none; core/memvar.c) */
  uint16_t borrowed;        /* its binding is an argument passed by reference (PARAMETERS) */
  uint16_t permanent;       /* its name is a module's (it holds none, and its entry stays when it ends) */
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

/* Where the call stack ends (the frame after its last, NULL for the bottom): in a task, below the task's own calls. */
static inline const xb_frame *xb_stack_end(const xb_ctx *ctx) { return ctx->task_depth ? ctx->task_base : NULL; }

/* SET DECIMALS as an item's decimal count (items keep 16 bits, as in Clipper). */
static inline uint16_t xb_decimals(const xb_ctx *ctx) { return (uint16_t)ctx->set_decimals; }

#endif
