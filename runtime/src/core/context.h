/* Thread contexts and the environments of flows. */
#ifndef XB_CONTEXT_H
#define XB_CONTEXT_H

#include "gc/gc.h"

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
  char    *date_format;     /* SET DATE FORMAT picture (malloc'ed, NUL-terminated) */
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
  xb_item **roots;          /* temporary C roots (xb_gc_root_push) */
  unsigned rootLen, rootCap;
  xb_item  break_value;     /* the value of a pending BREAK (a root of the collector) */
  uint32_t error_depth;     /* error blocks running */
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

void xb_ctx_set_date_format(xb_ctx *ctx, const char *s, size_t len);

/* The lowest address of the stack of the context's thread that the program may use: compiled code raises the stack
 * error a reserve above it (core/ctx.c); NULL: unknown, no limit. */
void xb_ctx_set_stack(xb_ctx *ctx, const char *low);

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
