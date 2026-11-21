/* Compiled modules register themselves before main(): their STATICs become GC
 * roots; their initialization and their INIT and EXIT procedures run around
 * the program's start function (see xb_program_run in xbrt.h). */
#include "xbrt_int.h"

#include "core/sched.h"

#include "rdd/rdd.h"

#include <stdlib.h>

typedef struct xb_module {
  struct xb_module *next, *prev;
  xb_module_init init;
  const xb_func *procs; /* `ninit` INIT procedures, then `nexit` EXIT ones */
  uint32_t ninit, nexit;
  uint32_t level;       /* XB_LEVEL_PROGRAM in xbrt.h */
} xb_module;

/* By level, then in registration order (the system's order of the modules' constructors). */
static xb_module *modules, *modulesLast;

void xb_module_register(xb_module_init init, xb_item *statics, uint32_t nstatics, const xb_func *procs,
                        uint32_t ninit, uint32_t nexit, const xb_symbol *symbols, uint32_t nsymbols,
                        xb_memvar *memvars, uint32_t nmemvars, uint32_t level) {
  if (nstatics) xb_gc_add_root(statics, nstatics);
  xb_symbols_add(symbols, nsymbols, level);
  xb_memvars_add(memvars, nmemvars);
  xb_module *m = malloc(sizeof *m);
  if (!m) xb_gc_fatal("out of memory");
  m->init = init;
  m->procs = procs;
  m->ninit = ninit;
  m->nexit = nexit;
  m->level = level;
  xb_module *after = modulesLast; /* the last module of this level or a lower one */
  while (after && after->level > level) after = after->prev;
  m->prev = after;
  m->next = after ? after->next : modules;
  if (m->next) m->next->prev = m;
  else modulesLast = m;
  if (after) after->next = m;
  else modules = m;
}

/* Whether the function just called asks to unwind (a safepoint only pauses). */
static int unwinding(xb_ctx *ctx) { return xb_pending_get(ctx) && xb_pending(ctx); }

/* Calls `fn` with the arguments, its result in a rooted item and dropped. */
/* Calls `fn`; with `start` (the start function), a result that is a task (an asynchronous start procedure's) is
 * waited for. */
static void call_as(xb_ctx *ctx, xb_func fn, uint32_t argc, xb_item *argv, int start) {
  xb_item ret;
  xb_set_nil(&ret);
  xb_gc_root_push(ctx, &ret);
  fn(ctx, &ret, argc, argv);
  if (start && !(xb_pending_get(ctx) & XB_PENDING_UNWIND)) xb_task_await_start(ctx, &ret);
  xb_gc_root_pop(ctx, 1);
}

static void call(xb_ctx *ctx, xb_func fn, uint32_t argc, xb_item *argv) { call_as(ctx, fn, argc, argv, 0); }

/* Calls `fn` with a rooted copy of the arguments: a function may replace the
 * arguments it received (PValue()), and the next one receives them as given. */
static void call_with_copy(xb_ctx *ctx, xb_func fn, uint32_t argc, const xb_item *argv) {
  xb_item *copy = argc ? malloc(argc * sizeof *copy) : NULL;
  if (argc && !copy) xb_gc_fatal("out of memory");
  for (uint32_t i = 0; i < argc; ++i) {
    copy[i] = argv[i];
    xb_gc_root_push(ctx, &copy[i]);
  }
  call(ctx, fn, argc, copy);
  xb_gc_root_pop(ctx, argc);
  free(copy);
}

/* Up to the start function; 0 when a request to unwind stopped it. */
static int start_up(xb_ctx *ctx, uint32_t argc, xb_item *argv) {
  for (const xb_module *m = modules; m; m = m->next) {
    m->init(ctx);
    if (unwinding(ctx)) return 0;
  }
  /* ErrorSys() by name: the program's own when it defines one, else the runtime's (rtl/errorsys.prg). */
  const xb_symbol_entry *errorsys = xb_symbol_find("ERRORSYS", 8);
  if (errorsys) {
    call(ctx, errorsys->ref.fn, 0, argv); /* no arguments; argv stays a valid address */
    if (unwinding(ctx)) return 0;
  }
  for (const xb_module *m = modules; m; m = m->next)
    for (uint32_t k = m->ninit; k-- > 0;) {
      call_with_copy(ctx, m->procs[k], argc, argv);
      if (unwinding(ctx)) return 0;
    }
  return 1;
}

void xb_program_run(xb_ctx *ctx, xb_func start, uint32_t argc, xb_item *argv) {
  xb_symbols_seal();
  if (start_up(ctx, argc, argv)) call_as(ctx, start, argc, argv, 1);
  xb_threads_end(ctx);
  xb_pending_clear(ctx, XB_PENDING_UNWIND); /* the request is served: the exit code stays */
  xb_set_nil(&ctx->break_value);
  if (xb_gc_finalizable_count()) { /* what the start function alone kept: destroyed before the EXIT procedures */
    xb_gc_collect();
    xb_pending_clear(ctx, XB_PENDING_UNWIND);
    xb_classes_finalize(ctx);
    xb_pending_clear(ctx, XB_PENDING_UNWIND);
  }
  /* From the highest level down (the program's modules, then the libraries that the ones above use), each level's
   * modules in registration order. */
  int stopped = 0;
  for (const xb_module *last = modulesLast; last && !stopped;) {
    const xb_module *first = last;
    while (first->prev && first->prev->level == last->level) first = first->prev;
    for (const xb_module *m = first; m != last->next && !stopped; m = m->next)
      for (uint32_t k = 0; k < m->nexit && !stopped; ++k) {
        call(ctx, m->procs[m->ninit + k], 0, argv); /* no arguments; argv stays a valid address */
        stopped = unwinding(ctx);
      }
    last = first->prev;
  }
  xb_threads_end(ctx); /* those the EXIT procedures started */
  xb_env_reap_all(ctx); /* the asynchronous functions still suspended: their tables too */
  xb_env_close_areas(ctx); /* the tables are closed whatever ended the program */
}
