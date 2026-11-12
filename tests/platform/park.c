/* Waiting on an address: a value other than the expected one returns at once; two threads hand a turn back and forth
 * through park and wake; a wait with nobody to wake it times out after the time asked; waking one wakes exactly the
 * waiter that came first, and waking all wakes the others. */
#include "check.h"
#include "core/sync.h"

#define TURNS 20000u
#define WAITERS 6

static uint32_t turn, gate, arrived, left, order[WAITERS], leaving;

static void player(void *arg) {
  uint32_t me = (uint32_t)(size_t)arg;
  for (uint32_t i = 0; i < TURNS; ++i) {
    uint32_t t;
    while ((t = xb_atomic_load32(&turn, XB_ACQUIRE)) % 2 != me) xb_park_wait(&turn, t, -1);
    xb_atomic_store32(&turn, t + 1, XB_RELEASE);
    xb_park_wake(&turn, 0);
  }
}

/* Waits at the gate, after the waiters before it (each one waits for its turn to arrive), and records its place in
 * the order of leaving. */
static void wait_gate(void *arg) {
  uint32_t me = (uint32_t)(size_t)arg;
  while (xb_atomic_load32(&arrived, XB_ACQUIRE) != me) xb_platform_sleep_ms(1);
  xb_platform_sleep_ms(20); /* the one before is asleep by now, not only counted */
  xb_atomic_add32(&arrived, 1, XB_RELEASE);
  while (!xb_atomic_load32(&gate, XB_ACQUIRE)) xb_park_wait(&gate, 0, -1);
  order[me] = xb_atomic_add32(&leaving, 1, XB_ACQ_REL);
  xb_atomic_add32(&left, 1, XB_RELEASE);
}

int main(void) {
  xb_platform_init();
  uint32_t value = 5;
  CHECK(xb_park_wait(&value, 4, -1) == 1);

  xb_thread a, b;
  CHECK(xb_thread_start(&a, 0, player, (void *)0) && xb_thread_start(&b, 0, player, (void *)1));
  xb_thread_join(a);
  xb_thread_join(b);
  CHECK(turn == 2 * TURNS);

  int timeouts = 0; /* a wait may end early for no reason, but not every one of a hundred */
  for (int i = 0; i < 100 && timeouts < 3; ++i) {
    int64_t before = xb_platform_monotonic_ns();
    if (!xb_park_wait(&value, 5, 5 * XB_NS_PER_MS)) {
      CHECK(xb_platform_monotonic_ns() - before >= 5 * XB_NS_PER_MS);
      ++timeouts;
    }
  }
  CHECK(timeouts == 3);
  CHECK(xb_park_wake(&value, 0) == 0);

  xb_thread waiters[WAITERS];
  for (size_t k = 0; k < WAITERS; ++k) CHECK(xb_thread_start(&waiters[k], 0, wait_gate, (void *)k));
  while (xb_atomic_load32(&arrived, XB_ACQUIRE) < WAITERS) xb_platform_sleep_ms(1);
  xb_platform_sleep_ms(50); /* the last one is asleep by now */
  xb_atomic_store32(&gate, 1, XB_RELEASE);
  CHECK(xb_park_wake(&gate, 0) == 1);
  while (xb_atomic_load32(&left, XB_ACQUIRE) < 1) xb_platform_sleep_ms(1);
  xb_platform_sleep_ms(50); /* time for any other to leave, had it been woken */
  CHECK(xb_atomic_load32(&left, XB_ACQUIRE) == 1 && order[0] == 0);
  CHECK(xb_park_wake(&gate, 1) == WAITERS - 1);
  for (size_t k = 0; k < WAITERS; ++k) xb_thread_join(waiters[k]);
  CHECK(left == WAITERS);
  puts("park ok");
  return 0;
}
