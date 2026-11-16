/* Publication in the middle of an operation (platform.publish_array, platform.publish_hash): the program's storage
 * becomes shared when a second thread attaches, every other thread stopped at a safepoint, and a large allocation is
 * one. This thread holds an object in a root (as a STATIC holds it), local until then; another thread attaches, which
 * asks this one to stop; this one, the request seen, runs an operation that grows the object, whose allocation is
 * where it stops: the object is shared when the allocation returns, and the operation must finish as for a shared
 * object (the value it stores shared too), not in place as it began. */
#ifndef XB_TEST_PUBLISH_H
#define XB_TEST_PUBLISH_H

#include "check.h"
#include "core/shared.h"
#include "xbrt_int.h"

static void attach_and_go(void *unused) {
  xb_gc_thread_attach(&unused); /* publishes the program's storage: waits for the first thread's safepoint */
  xb_gc_thread_detach();
}

static void join_second(void *t) { xb_thread_join(*(xb_thread *)t); }

/* Starts the second thread and returns once its attach has asked this thread to stop: the next safepoint publishes. */
static xb_thread publish_soon(xb_ctx *ctx) {
  CHECK(!xb_storage_is_shared());
  xb_thread t;
  CHECK(xb_thread_start(&t, (size_t)8 << 20, attach_and_go, NULL));
  while (!(xb_pending_get(ctx) & XB_PENDING_SAFEPOINT)) xb_thread_yield(); /* no safepoint here */
  return t;
}

/* Joins the second thread (a blocking operation: no collection waits for this one). */
static void publish_done(xb_ctx *ctx, xb_thread t) {
  CHECK(xb_storage_is_shared());
  if (xb_pending_get(ctx)) xb_pending_gc(ctx);
  xb_blocking_run(join_second, &t);
}

#endif
