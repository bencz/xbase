/* The program's storage once a second thread attaches (docs/concurrency.md, publication; xb_storage_shared): the
 * attach stops the first thread and shares what the STATICs, the class variables and the memory variables hold (an
 * array a STATIC held before is shared from then on), and threads then run compiled code that reads and writes
 * STATICs (one moved into a cell meanwhile, captured by a codeblock; one passed by reference; one a FOR EACH variable),
 * a class variable of a class prepared before, and that array's elements, while this thread appends to an array a
 * STATIC holds (shared in the middle of its appends), collections running: no reader sees an item read torn, and the
 * appends all land (shared_storage.prg). */
#include "check.h"
#include "core/shared.h"
#include "xbrt_int.h"

#include <stdio.h>

void xbp_STORAGESTART(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_STORAGEWRITER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_STORAGEREADER(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_STORAGEGROW(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

#define GROWN 200000

#define WRITERS 2
#define READERS 2
#define ROUNDS 8000

static uint32_t bad;

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
  CHECK(!xb_pending_get(w->ctx)); /* no error was raised */
  xb_gc_root_pop(w->ctx, 3);
  xb_ctx_free(w->ctx);
  xb_gc_thread_detach();
}

static void writer(void *arg) {
  worker w;
  begin(&w, &arg);
  xb_set_int(&w.args[0], ROUNDS);
  xb_set_int(&w.args[1], (int64_t)(size_t)arg);
  xbp_STORAGEWRITER(w.ctx, &w.ret, 2, w.args);
  end(&w);
}

static void reader(void *arg) {
  worker w;
  begin(&w, &arg);
  xb_set_int(&w.args[0], ROUNDS);
  xb_set_int(&w.args[1], (int64_t)(size_t)arg);
  xbp_STORAGEREADER(w.ctx, &w.ret, 2, w.args);
  CHECK(w.ret.type == XB_T_INT);
  xb_atomic_add32(&bad, (uint32_t)w.ret.v.i, XB_RELAXED);
  end(&w);
}

/* The main thread waits for the others in a blocking operation: an attach, or a collection, never waits for it. */
static void join_all(void *threads) {
  for (size_t k = 0; k < WRITERS + READERS; ++k) xb_thread_join(((xb_thread *)threads)[k]);
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_item array;
  xb_set_nil(&array);
  xb_gc_root_push(ctx, &array);
  xbp_STORAGESTART(ctx, &array, 0, NULL);
  CHECK(array.type == XB_T_ARRAY && !xb_is_shared(array.v.p) && !xb_storage_is_shared()); /* one thread: local */
  xb_thread threads[WRITERS + READERS];
  for (size_t k = 0; k < WRITERS; ++k) CHECK(xb_thread_start(&threads[k], (size_t)8 << 20, writer, (void *)k));
  for (size_t k = 0; k < READERS; ++k)
    CHECK(xb_thread_start(&threads[WRITERS + k], (size_t)8 << 20, reader, (void *)k));
  xb_item n, grown;
  xb_set_int(&n, GROWN);
  xb_set_nil(&grown);
  xb_gc_root_push(ctx, &grown);
  xbp_STORAGEGROW(ctx, &grown, 1, &n); /* while the others attach: shared in the middle of its appends */
  CHECK(grown.type == XB_T_INT && grown.v.i == GROWN);
  xb_gc_root_pop(ctx, 1);
  xb_blocking_run(join_all, threads);
  CHECK(xb_storage_is_shared() && xb_is_shared(array.v.p)); /* published when the second thread attached */
  if (bad) fprintf(stderr, "%u items read that no writer stored\n", bad);
  CHECK(bad == 0);
  xb_gc_root_pop(ctx, 1);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  puts("shared_storage ok");
  return 0;
}
