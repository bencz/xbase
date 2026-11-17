#include "core/sync.h"

#include <stdlib.h>

static uint32_t lastNumber;
static XB_THREAD_LOCAL uint32_t threadNumber;
static XB_THREAD_LOCAL xb_flow *innermost;

static uint32_t number_new(void) {
  uint32_t n;
  while ((n = xb_atomic_add32(&lastNumber, 1, XB_RELAXED) + 1) == 0) {} /* 0 is no owner */
  return n;
}

uint32_t xb_thread_self(void) {
  if (!threadNumber) threadNumber = number_new();
  return threadNumber;
}

uint32_t xb_flow_self(void) {
  if (!innermost) return xb_thread_self();
  if (!innermost->number) innermost->number = number_new(); /* most flows take no lock: numbered when one does */
  return innermost->number;
}

void xb_flow_enter(xb_flow *flow) {
  flow->outer = innermost;
  innermost = flow;
}

void xb_flow_leave(xb_flow *flow) { innermost = flow->outer; }

int xb_flow_outer(uint32_t owner) {
  if (!owner || !innermost) return 0; /* the thread's own flow runs inside none */
  if (owner == threadNumber) return 1;
  for (const xb_flow *f = innermost->outer; f; f = f->outer)
    if (f->number == owner) return 1;
  return 0;
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
  uint32_t waiting; /* threads parked (or about to) in it: a wake that finds none takes no lock */
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
  xb_atomic_add32(&b->waiting, 1, XB_SEQ_CST); /* before the value is read: a waker that changed it sees this */
  if (xb_atomic_load32(addr, XB_SEQ_CST) != expected) {
    xb_atomic_add32(&b->waiting, (uint32_t)-1, XB_RELAXED);
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
  xb_atomic_add32(&b->waiting, (uint32_t)-1, XB_RELAXED);
  xb_mutex_unlock(&b->lock);
  xb_cond_destroy(&me.wake);
  return me.woken || left > 0;
}

uint32_t xb_park_wake(const uint32_t *addr, int all) {
  bucket *b = bucket_of(addr);
  xb_atomic_fence(XB_SEQ_CST); /* the value the caller changed, before the count is read: see xb_park_wait */
  if (!xb_atomic_load32(&b->waiting, XB_SEQ_CST)) return 0;
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

typedef struct park_args {
  const uint32_t *addr;
  uint32_t expected;
  int64_t ns;
} park_args;

static void park(void *p) {
  const park_args *a = p;
  xb_park_wait(a->addr, a->expected, a->ns);
}

void xb_park_wait_blocking(const uint32_t *addr, uint32_t expected, int64_t ns) {
  park_args a = {addr, expected, ns};
  xb_blocking_run(park, &a);
}

void xb_park_wait_short(const uint32_t *addr, uint32_t expected, int64_t ns) {
  park_args a = {addr, expected, ns};
  xb_waiting_run(park, &a);
}

/* The holder's number is written only by the holder, so a caller that reads its own there holds the lock. */
static void hold(xb_rwords w, uint32_t self) {
  xb_atomic_store32(w.owner, self, XB_RELAXED);
  *w.depth = 1;
}

int xb_rwords_try(xb_rwords w, uint32_t self) {
  if (xb_atomic_load32(w.owner, XB_RELAXED) == self) {
    ++*w.depth;
    return 1;
  }
  uint32_t free_state = 0;
  if (!xb_atomic_cas32(w.state, &free_state, 1, XB_ACQUIRE)) return 0;
  hold(w, self);
  return 1;
}

int xb_rwords_claim(xb_rwords w, uint32_t self) {
  if (xb_atomic_xchg32(w.state, 2, XB_ACQUIRE) != 0) return 0;
  hold(w, self);
  return 1;
}

int xb_rwords_release(xb_rwords w, uint32_t self) {
  if (xb_atomic_load32(w.owner, XB_RELAXED) != self) return 0;
  if (--*w.depth) return 1;
  xb_atomic_store32(w.owner, 0, XB_RELAXED);
  if (xb_atomic_xchg32(w.state, 0, XB_RELEASE) == 2) xb_park_wake(w.state, 0);
  return 1;
}

static xb_rwords rlock_words(xb_rlock *l) { return (xb_rwords){&l->state, &l->owner, &l->depth}; }

void xb_rlock_lock(xb_rlock *l) {
  xb_rwords w = rlock_words(l);
  uint32_t self = xb_thread_self();
  if (xb_rwords_try(w, self)) return;
  while (!xb_rwords_claim(w, self)) xb_park_wait_blocking(&l->state, 2, -1); /* its holder may wait for a collection */
}

void xb_rlock_unlock(xb_rlock *l) {
  if (!xb_rwords_release(rlock_words(l), xb_thread_self())) abort(); /* only its holder releases it */
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

static xb_mutex output;
static xb_once_flag outputMade = XB_ONCE_INIT;

static void make_output_lock(void) {
  if (!xb_mutex_init(&output)) abort();
}

void xb_output_lock(void) {
  xb_once(&outputMade, make_output_lock);
  xb_mutex_lock(&output);
}

void xb_output_unlock(void) { xb_mutex_unlock(&output); }
