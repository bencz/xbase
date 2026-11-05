/* Compiled modules register themselves before main(): their STATICs become GC
 * roots, and their initialization runs before the program's first function. */
#include "xbrt_int.h"

#include <stdlib.h>

typedef struct xb_module {
  struct xb_module *next;
  xb_module_init init;
} xb_module;

static xb_module *modules, **modulesTail = &modules;

void xb_module_register(xb_module_init init, xb_item *statics, uint32_t nstatics) {
  if (nstatics) xb_gc_add_root(statics, nstatics);
  xb_module *m = malloc(sizeof *m);
  if (!m) xb_gc_fatal("out of memory");
  m->next = NULL;
  m->init = init;
  *modulesTail = m; /* registration order */
  modulesTail = &m->next;
}

void xb_modules_init(xb_ctx *ctx) {
  for (const xb_module *m = modules; m; m = m->next) {
    m->init(ctx);
    if (ctx->pending && xb_pending(ctx)) return;
  }
}
