/* Compiled code's fast paths when what they reach is shared (docs/concurrency.md, shared objects, step 3): threads run
 * the xBase functions of shared_compiled.prg on one shared graph (an array, codeblocks over captured variables, a
 * hash), writers storing while readers read, collections (full ones, which move objects, too) running meanwhile: no
 * reader sees an item read torn, a string half appended or a key that is not one. */
#include "check.h"
#include "core/shared.h"
#include "xbrt_int.h"

#include <stdio.h>

void xbp_NEWSHARED(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_WRITER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_READER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

#define WRITERS 2
#define READERS 2
#define ROUNDS 20000

static xb_item shared;
static uint32_t bad;

typedef struct worker {
  xb_ctx *ctx;
  xb_item args[3], ret;
} worker;

static void begin(worker *w, void *base) {
  xb_gc_thread_attach(base);
  w->ctx = xb_ctx_new();
  for (int k = 0; k < 3; ++k) {
    xb_set_nil(&w->args[k]);
    xb_gc_root_push(w->ctx, &w->args[k]);
  }
  xb_set_nil(&w->ret);
  xb_gc_root_push(w->ctx, &w->ret);
}

static void end(worker *w) {
  CHECK(!xb_pending_get(w->ctx)); /* no error was raised */
  xb_gc_root_pop(w->ctx, 4);
  xb_ctx_free(w->ctx);
  xb_gc_thread_detach();
}

static void writer(void *arg) {
  worker w;
  begin(&w, &arg);
  w.args[0] = shared;
  xb_set_int(&w.args[1], ROUNDS);
  xb_set_int(&w.args[2], (int64_t)(size_t)arg);
  xbp_WRITER(w.ctx, &w.ret, 3, w.args);
  end(&w);
}

static void reader(void *arg) {
  worker w;
  begin(&w, &arg);
  w.args[0] = shared;
  xbp_READER(w.ctx, &w.ret, 1, w.args);
  CHECK(w.ret.type == XB_T_INT);
  xb_atomic_add32(&bad, (uint32_t)w.ret.v.i, XB_RELAXED);
  end(&w);
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_set_nil(&shared);
  xb_gc_add_root(&shared, 1);
  xbp_NEWSHARED(ctx, &shared, 0, NULL);
  CHECK(shared.type == XB_T_ARRAY);
  xb_share(&shared);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  xb_thread threads[WRITERS + READERS];
  for (size_t k = 0; k < READERS; ++k) CHECK(xb_thread_start(&threads[WRITERS + k], (size_t)8 << 20, reader, NULL));
  for (size_t k = 0; k < WRITERS; ++k) CHECK(xb_thread_start(&threads[k], (size_t)8 << 20, writer, (void *)k));
  for (size_t k = 0; k < WRITERS; ++k) xb_thread_join(threads[k]);
  xb_gc_thread_attach(&base);
  xb_item done;
  xb_set_logical(&done, 0);
  CHECK(xb_array_put(shared.v.p, 3, &done)); /* the readers stop */
  xb_gc_thread_detach();
  for (size_t k = 0; k < READERS; ++k) xb_thread_join(threads[WRITERS + k]);
  if (bad) fprintf(stderr, "%u items read that no writer stored\n", bad);
  CHECK(bad == 0);
  puts("shared_compiled ok");
  return 0;
}
