/* An object's variables when the object is shared (types/object.c), threads at once, collections (full ones, which move
 * objects, too) running meanwhile: writers assign an Error object's `cargo` (any value: alternating integers and
 * doubles) and `description` (a string variable) by messages while readers read them back: a reader never sees an
 * integer of a double's bits or a tiny double (an item read torn), nor anything but a string in `description`. */
#include "check.h"
#include "core/shared.h"
#include "xbrt_int.h"

#include <stdio.h>

void xbf_ERRORNEW(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

#define WRITERS 2
#define READERS 2
#define ROUNDS 40000u

static xb_item shared;
static uint32_t bad, busy;

typedef struct worker {
  xb_ctx *ctx;
  xb_item args[2], ret;
} worker;

static void begin(worker *w, void *base) {
  xb_gc_thread_attach(base);
  w->ctx = xb_ctx_new();
  for (int k = 0; k < 2; ++k) {
    xb_set_nil(&w->args[k]);
    xb_gc_root_push(w->ctx, &w->args[k]);
  }
  xb_set_nil(&w->ret);
  xb_gc_root_push(w->ctx, &w->ret);
}

static void end(worker *w) {
  xb_gc_root_pop(w->ctx, 3);
  xb_ctx_free(w->ctx);
  xb_gc_thread_detach();
}

static void poll(worker *w, uint32_t n) {
  if (xb_pending_get(w->ctx)) xb_pending_gc(w->ctx);
  if (n % 8192u == 4096u) xb_gc_collect(); /* full: objects may move */
  else if (n % 2048u == 0) xb_gc_collect_young();
}

static void writer(void *arg) {
  worker w;
  begin(&w, &arg);
  uint32_t me = (uint32_t)(size_t)arg;
  for (uint32_t j = 0; j < ROUNDS; ++j) {
    w.args[0] = shared;
    if ((j / 8u + me) & 1u) xb_set_int(&w.args[1], (int64_t)j);
    else xb_set_double(&w.args[1], 0.5 + (double)j, 1);
    xb_op_send(w.ctx, &w.ret, "_CARGO", 2, w.args);
    if (j % 16u == me) {
      char text[32];
      int n = snprintf(text, sizeof text, "text %u", j);
      xb_set_str(&w.args[1], xb_str_new(text, (size_t)n)); /* shared as it is stored */
      xb_op_send(w.ctx, &w.ret, "_DESCRIPTION", 2, w.args);
    }
    poll(&w, j);
  }
  xb_atomic_add32(&busy, (uint32_t)-1, XB_RELEASE);
  end(&w);
}

static void reader(void *arg) {
  worker w;
  begin(&w, &arg);
  for (uint32_t n = 0; xb_atomic_load32(&busy, XB_ACQUIRE); ++n) {
    w.args[0] = shared;
    xb_op_send(w.ctx, &w.ret, "CARGO", 1, w.args);
    if (w.ret.type == XB_T_INT ? (uint64_t)w.ret.v.i >= ((uint64_t)1 << 32)
                               : w.ret.type == XB_T_DOUBLE ? !(w.ret.v.d >= 0.5) : w.ret.type != XB_T_NIL)
      xb_atomic_add32(&bad, 1, XB_RELAXED);
    xb_op_send(w.ctx, &w.ret, "DESCRIPTION", 1, w.args);
    if (w.ret.type != XB_T_STRING) xb_atomic_add32(&bad, 1, XB_RELAXED);
    poll(&w, n);
  }
  end(&w);
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_set_nil(&shared);
  xb_gc_add_root(&shared, 1);
  xbf_ERRORNEW(ctx, &shared, 0, NULL);
  CHECK(shared.type == XB_T_OBJECT);
  xb_share(&shared);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  xb_thread threads[WRITERS + READERS];
  xb_atomic_store32(&busy, WRITERS, XB_RELEASE);
  for (size_t k = 0; k < WRITERS; ++k) CHECK(xb_thread_start(&threads[k], (size_t)8 << 20, writer, (void *)k));
  for (size_t k = 0; k < READERS; ++k) CHECK(xb_thread_start(&threads[WRITERS + k], (size_t)8 << 20, reader, NULL));
  for (size_t k = 0; k < WRITERS + READERS; ++k) xb_thread_join(threads[k]);
  CHECK(bad == 0);
  puts("shared_objects ok");
  return 0;
}
