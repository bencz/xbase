/* Workers in operations that may block (xb_blocking_run: a file's I/O), with one worker running tasks at once
 * (blocking_calls.prg): one whose call blocks 400 ms has its place given to another worker (one place handed over at
 * least), which runs the tasks queued behind it long before the call returns; calls that return soon (5 microseconds
 * of work) keep the worker's place, so 32 tasks making 128000 of them, queued all along, hand few places over: only
 * when the system holds a worker inside a call past the monitor's 50 microseconds (a loaded machine), however often
 * the monitor looks (timers added meanwhile wake it again and again). A place handed over for each call, or for each
 * call seen at two looks however close, would make thousands. */
#include "check.h"
#include "core/sched.h"
#include "xbrt_int.h"

#include <stdio.h>

void xbp_BLOCKEDCALL(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_SHORTCALLS(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);
void xbp_BLOCKFOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv);

static uint32_t inside, most; /* the short calls running now, and the most that ran at once */

/* Sleeps n > 0 milliseconds, or keeps the processor -n microseconds (a short call's work). */
static void sleep_for(void *arg) {
  int64_t n = *(const int64_t *)arg;
  if (n > 0) {
    xb_platform_sleep_ms((uint32_t)n);
    return;
  }
  uint32_t now = xb_atomic_add32(&inside, 1, XB_ACQ_REL) + 1, seen = xb_atomic_load32(&most, XB_RELAXED);
  while (now > seen && !xb_atomic_cas32(&most, &seen, now, XB_RELAXED)) {
  }
  int64_t until = xb_platform_monotonic_ns() - n * XB_NS_PER_US;
  while (xb_platform_monotonic_ns() < until) {
  }
  xb_atomic_add32(&inside, (uint32_t)-1, XB_ACQ_REL);
}

/* BlockFor( n ): sleep_for( n ) as an operation that may block. */
void xbp_BLOCKFOR(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  (void)ctx;
  int64_t n = argc && argv[0].type == XB_T_INT ? argv[0].v.i : 0;
  xb_blocking_run(sleep_for, &n);
  xb_set_nil(ret);
}

int main(void) {
  xb_platform_init();
  xb_sched_set_workers(1);
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  xb_item r;
  xb_set_nil(&r);
  xb_gc_root_push(ctx, &r);
  uint32_t handed = xb_sched_handoffs();
  xbp_BLOCKEDCALL(ctx, &r, 0, NULL);
  CHECK(!xb_pending_get(ctx) && r.type == XB_T_ARRAY);
  const xb_item *times = xb_array_items(r.v.p);
  printf("queued tasks ended after %lld ms, the blocked call after %lld ms\n", (long long)times[0].v.i,
         (long long)times[1].v.i);
  CHECK(times[1].v.i >= 400);
  CHECK(times[0].v.i < 300);
  CHECK(xb_sched_handoffs() - handed >= 1);
  handed = xb_sched_handoffs();
  xbp_SHORTCALLS(ctx, &r, 0, NULL);
  CHECK(!xb_pending_get(ctx) && r.type == XB_T_INT);
  handed = xb_sched_handoffs() - handed;
  printf("short calls: %u places handed over, %u calls at once at most, %lld workers alive\n", handed, most,
         (long long)r.v.i);
  CHECK(handed < 1000);
  xb_gc_root_pop(ctx, 1);
  xb_threads_end(ctx);
  xb_ctx_free(ctx);
  puts("blocking_calls ok");
  return 0;
}
