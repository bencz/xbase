/* Whom a collection asks to stop at a safepoint: the contexts of the other threads running xBase code. Never the
 * collecting thread's own (a request served for nothing sends compiled loops to their generic copies) nor that of a
 * thread blocked in xb_blocking_run (it parks on its way back if the collection still runs). And whom it asks to run
 * destructors: the context that runs them, or the one that takes its place when it goes, objects waiting. */
#include "check.h"
#include "xbrt_int.h"

enum { RUNNING = 1, TO_BLOCK, BLOCKED, DONE };

static uint32_t phase;
static uint32_t served;        /* safepoint requests the other thread saw while it ran */
static uint32_t askedBlocked;  /* the other thread found a request after its blocking operation */

static void await_phase(uint32_t want) {
  while (xb_atomic_load32(&phase, XB_ACQUIRE) != want) xb_platform_sleep_ms(1);
}

static void blocked(void *unused) {
  (void)unused;
  xb_atomic_store32(&phase, BLOCKED, XB_RELEASE);
  await_phase(DONE);
}

static void other(void *arg) {
  xb_gc_thread_attach(&arg);
  xb_ctx *ctx = xb_ctx_new();
  xb_atomic_store32(&phase, RUNNING, XB_RELEASE);
  while (xb_atomic_load32(&phase, XB_ACQUIRE) == RUNNING) { /* polls, as compiled code makes */
    uint32_t bits = xb_pending_get(ctx);
    if (bits & XB_PENDING_SAFEPOINT) xb_atomic_add32(&served, 1, XB_RELAXED);
    if (bits) xb_pending_gc(ctx);
  }
  /* The collection is over (the main thread moved on after it), but its collector raises the request again each time
   * its wait ends: one raised after this thread served it, before it parked, stays, and is served here. */
  xb_pending_gc(ctx);
  xb_blocking_run(blocked, NULL);
  if (xb_pending_get(ctx) & XB_PENDING_SAFEPOINT) xb_atomic_store32(&askedBlocked, 1, XB_RELAXED);
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

int main(void) {
  xb_platform_init();
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *first = xb_ctx_new(); /* runs the destructors */
  xb_ctx *ctx = xb_ctx_new();
  xb_gc_finalizable(xb_array_alloc(1)); /* unreachable */
  xb_gc_collect();
  CHECK(xb_pending_get(first) & XB_PENDING_FINALIZE);
  CHECK(!(xb_pending_get(ctx) & XB_PENDING_FINALIZE));
  xb_ctx_free(first); /* before it ran them */
  CHECK(xb_pending_get(ctx) & XB_PENDING_FINALIZE);
  xb_item done;
  while (xb_gc_next_finalized(&done)) {}
  xb_pending_clear(ctx, XB_PENDING_FINALIZE);

  xb_gc_collect(); /* alone */
  CHECK(!(xb_pending_get(ctx) & XB_PENDING_SAFEPOINT));

  xb_thread thread;
  CHECK(xb_thread_start(&thread, (size_t)8 << 20, other, NULL));
  await_phase(RUNNING);
  xb_gc_collect(); /* the other thread runs: it is asked, and it serves the request */
  CHECK(xb_atomic_load32(&served, XB_RELAXED) >= 1);
  CHECK(!(xb_pending_get(ctx) & XB_PENDING_SAFEPOINT));

  xb_atomic_store32(&phase, TO_BLOCK, XB_RELEASE);
  await_phase(BLOCKED);
  xb_gc_collect(); /* the other thread is blocked: not asked */
  xb_atomic_store32(&phase, DONE, XB_RELEASE);
  xb_thread_join(thread);
  CHECK(!xb_atomic_load32(&askedBlocked, XB_RELAXED));
  CHECK(!(xb_pending_get(ctx) & XB_PENDING_SAFEPOINT));

  xb_ctx_free(ctx);
  xb_gc_thread_detach();
  puts("gc_requests ok");
  return 0;
}
