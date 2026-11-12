#include "core/sync.h"

#include <stdlib.h>

uint32_t xb_thread_self(void) {
  static uint32_t last;
  static XB_THREAD_LOCAL uint32_t self;
  if (!self) self = xb_atomic_add32(&last, 1, XB_RELAXED) + 1;
  return self;
}

/* The parking lot: a waiter queues in the bucket of its address, in arrival order, and sleeps on a condition of its
 * own, so that a wake reaches exactly the waiters it marks. Buckets are made when first used (a bucket's state: 0
 * none, 1 being made, 2 ready), since the lot serves xb_once itself. */
#define PARK_BUCKETS 256u
#define PARK_YIELDS 64u /* tries that yield before those that sleep, while another thread makes a bucket */

typedef struct parked {
  const uint32_t *addr;
  xb_cond wake;
  int woken;
  struct parked *next;
} parked;

typedef struct bucket {
  uint32_t state;
  xb_mutex lock;
  parked *head, *tail;
} bucket;

static bucket buckets[PARK_BUCKETS];

static bucket *bucket_of(const uint32_t *addr) {
  uintptr_t h = (uintptr_t)addr / sizeof *addr;
  h ^= h >> 7;
  bucket *b = &buckets[h % PARK_BUCKETS];
  uint32_t state = xb_atomic_load32(&b->state, XB_ACQUIRE);
  for (uint32_t tries = 0; state != 2; ++tries) {
    uint32_t none = 0;
    if (state == 0 && xb_atomic_cas32(&b->state, &none, 1, XB_ACQ_REL)) {
      if (!xb_mutex_init(&b->lock)) abort(); /* no system objects left: nothing can wait */
      xb_atomic_store32(&b->state, 2, XB_RELEASE);
    } else {
      xb_platform_sleep_ms(tries < PARK_YIELDS ? 0 : 1); /* another thread is making it */
    }
    state = xb_atomic_load32(&b->state, XB_ACQUIRE);
  }
  return b;
}

int xb_park_wait(const uint32_t *addr, uint32_t expected, int64_t ns) {
  bucket *b = bucket_of(addr);
  int64_t deadline = ns < 0 ? 0 : xb_platform_monotonic_ns() + (ns < XB_WAIT_NS_MAX ? ns : XB_WAIT_NS_MAX);
  parked me = {addr, NULL, 0, NULL};
  if (!xb_cond_init(&me.wake)) abort();
  xb_mutex_lock(&b->lock);
  if (xb_atomic_load32(addr, XB_ACQUIRE) != expected) {
    xb_mutex_unlock(&b->lock);
    xb_cond_destroy(&me.wake);
    return 1;
  }
  if (b->tail) b->tail->next = &me;
  else b->head = &me;
  b->tail = &me;
  int64_t left = 1;
  while (!me.woken && left > 0) { /* the deadline decides, not what a timed wait reports */
    if (ns < 0) {
      xb_cond_wait(&me.wake, &b->lock);
    } else {
      left = deadline - xb_platform_monotonic_ns();
      if (left > 0) xb_cond_wait_ns(&me.wake, &b->lock, left);
    }
  }
  parked *prev = NULL;
  for (parked *w = b->head; w != &me; w = w->next) prev = w;
  if (prev) prev->next = me.next;
  else b->head = me.next;
  if (b->tail == &me) b->tail = prev;
  xb_mutex_unlock(&b->lock);
  xb_cond_destroy(&me.wake);
  return me.woken || left > 0;
}

uint32_t xb_park_wake(const uint32_t *addr, int all) {
  bucket *b = bucket_of(addr);
  uint32_t n = 0;
  xb_mutex_lock(&b->lock);
  for (parked *w = b->head; w && (all || !n); w = w->next) {
    if (w->addr != addr || w->woken) continue;
    w->woken = 1;
    xb_cond_signal(&w->wake);
    ++n;
  }
  xb_mutex_unlock(&b->lock);
  return n;
}

void xb_once(xb_once_flag *flag, void (*fn)(void)) {
  uint32_t state = xb_atomic_load32(&flag->state, XB_ACQUIRE);
  if (state == 2) return;
  uint32_t none = 0;
  if (state == 0 && xb_atomic_cas32(&flag->state, &none, 1, XB_ACQ_REL)) {
    fn();
    xb_atomic_store32(&flag->state, 2, XB_RELEASE);
    xb_park_wake(&flag->state, 1);
    return;
  }
  while (xb_atomic_load32(&flag->state, XB_ACQUIRE) != 2) xb_park_wait(&flag->state, 1, -1);
}
