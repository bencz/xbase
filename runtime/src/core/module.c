/* Compiled modules register themselves before main(): their STATICs become GC
 * roots; their initialization and their INIT and EXIT procedures run around
 * the program's start function (see xb_program_run in xbrt.h). */
#include "xbrt_int.h"

#include <stdlib.h>

typedef struct xb_module {
  struct xb_module *next;
  xb_module_init init;
  const xb_func *procs; /* `ninit` INIT procedures, then `nexit` EXIT ones */
  uint32_t ninit, nexit;
} xb_module;

static xb_module *modules, **modulesTail = &modules;

void xb_module_register(xb_module_init init, xb_item *statics, uint32_t nstatics, const xb_func *procs,
                        uint32_t ninit, uint32_t nexit, const xb_symbol *symbols, uint32_t nsymbols) {
  if (nstatics) xb_gc_add_root(statics, nstatics);
  xb_symbols_add(symbols, nsymbols);
  xb_module *m = malloc(sizeof *m);
  if (!m) xb_gc_fatal("out of memory");
  m->next = NULL;
  m->init = init;
  m->procs = procs;
  m->ninit = ninit;
  m->nexit = nexit;
  *modulesTail = m; /* registration order */
  modulesTail = &m->next;
}

/* Whether the function just called asks to unwind (a safepoint only pauses). */
static int unwinding(xb_ctx *ctx) { return ctx->pending && xb_pending(ctx); }

/* Calls `fn` with the arguments, its result in a rooted item and dropped. */
static void call(xb_ctx *ctx, xb_func fn, uint32_t argc, xb_item *argv) {
  xb_item ret;
  xb_set_nil(&ret);
  xb_gc_root_push(ctx, &ret);
  fn(ctx, &ret, argc, argv);
  xb_gc_root_pop(ctx, 1);
}

/* Up to the start function; 0 when a request to unwind stopped it. */
static int start_up(xb_ctx *ctx, uint32_t argc, xb_item *argv) {
  for (const xb_module *m = modules; m; m = m->next) {
    m->init(ctx);
    if (unwinding(ctx)) return 0;
  }
  for (const xb_module *m = modules; m; m = m->next)
    for (uint32_t k = m->ninit; k-- > 0;) {
      call(ctx, m->procs[k], argc, argv);
      if (unwinding(ctx)) return 0;
    }
  return 1;
}

void xb_program_run(xb_ctx *ctx, xb_func start, uint32_t argc, xb_item *argv) {
  xb_symbols_seal();
  if (start_up(ctx, argc, argv)) call(ctx, start, argc, argv);
  ctx->pending = XB_PENDING_NONE; /* the request is served: the exit code stays */
  xb_set_nil(&ctx->break_value);
  for (const xb_module *m = modules; m; m = m->next)
    for (uint32_t k = 0; k < m->nexit; ++k) {
      call(ctx, m->procs[m->ninit + k], 0, argv); /* no arguments; argv stays a valid address */
      if (unwinding(ctx)) return;
    }
}
