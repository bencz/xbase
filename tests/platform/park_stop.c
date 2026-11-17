/* A blocking park that also ends on a stop bit (xb_park_wait_blocking_stop, the waits a QUIT ends): a waiter whose word
 * keeps its value ends once another thread sets the bit and wakes the word, whether it was asleep already or not yet
 * (the bit set and the word woken racing with the start of the wait, many times: no wake is lost, so no round hangs);
 * a bit outside the mask does not end it; a change of the word ends it as before. */
#include "check.h"
#include "core/sync.h"
#include "xbrt_int.h"

#define ROUNDS 3000u

static uint32_t word, stop, started, ended;

static void waiter(void *arg) {
  (void)arg;
  int base = 0;
  xb_gc_thread_attach(&base);
  xb_ctx *ctx = xb_ctx_new();
  for (uint32_t r = 0; r < ROUNDS; ++r) {
    while (xb_atomic_load32(&started, XB_ACQUIRE) != r) xb_thread_yield();
    while (!(xb_atomic_load32(&stop, XB_ACQUIRE) & 1)) xb_park_wait_blocking_stop(&word, 0, &stop, 1, -1);
    xb_atomic_store32(&ended, r + 1, XB_RELEASE);
  }
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

int main(void) {
  xb_platform_init();
  xb_thread t; /* this thread attaches after the waiter ended: it spins meanwhile, which no collection could stop */
  CHECK(xb_thread_start(&t, 0, waiter, NULL));
  for (uint32_t r = 0; r < ROUNDS; ++r) {
    xb_atomic_store32(&stop, 0, XB_SEQ_CST);
    xb_atomic_store32(&started, r, XB_RELEASE);
    if (r % 3 == 0) xb_platform_sleep_ms(r % 2); /* sometimes asleep already, sometimes not yet */
    xb_atomic_or32(&stop, 1, XB_SEQ_CST);
    xb_atomic_fence(XB_SEQ_CST);
    xb_park_wake(&word, 1);
    while (xb_atomic_load32(&ended, XB_ACQUIRE) != r + 1) xb_thread_yield();
  }
  xb_thread_join(t);

  int base = 0;
  xb_gc_thread_attach(&base);
  xb_atomic_store32(&stop, 2, XB_SEQ_CST); /* outside the mask: the wait goes on until its limit */
  int64_t before = xb_platform_monotonic_ns();
  xb_ctx *ctx = xb_ctx_new();
  xb_park_wait_blocking_stop(&word, 0, &stop, 1, 30 * XB_NS_PER_MS);
  CHECK(xb_platform_monotonic_ns() - before >= 30 * XB_NS_PER_MS);
  xb_atomic_store32(&word, 1, XB_SEQ_CST); /* another value: no wait */
  xb_park_wait_blocking_stop(&word, 0, &stop, 1, -1);
  xb_ctx_free(ctx);
  puts("park_stop ok");
  return 0;
}
