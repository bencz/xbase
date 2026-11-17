/* Recursive locks: a thread takes its lock again as often as it likes and keeps it until it has released it as many
 * times; other threads wait meanwhile, and increments made under the lock by many threads at once all add up. Holders'
 * numbers are compared whole: a holder whose number differs from the caller's only above its low 32 bits (the number
 * the caller's would come back as after 2^32 others, were they 32 bits) keeps the caller out. */
#include "check.h"
#include "core/sync.h"

#define THREADS 8
#define ROUNDS 20000u
#define DEPTH 3u

static xb_rlock lock = XB_RLOCK_INIT;
static uint32_t inside, total, overlaps, held, probed;

static void nest(uint32_t depth) {
  xb_rlock_lock(&lock);
  if (depth == DEPTH) {
    if (xb_atomic_add32(&inside, 1, XB_ACQ_REL) != 0) xb_atomic_add32(&overlaps, 1, XB_RELAXED);
    total = total + 1;
    xb_atomic_add32(&inside, (uint32_t)-1, XB_ACQ_REL);
  } else {
    nest(depth + 1);
  }
  xb_rlock_unlock(&lock);
}

static void work(void *arg) {
  (void)arg;
  for (uint32_t i = 0; i < ROUNDS; ++i) nest(1);
}

static void probe(void *arg) {
  (void)arg;
  xb_rlock_lock(&lock); /* waits until the main thread has released its every hold */
  probed = !xb_atomic_load32(&held, XB_ACQUIRE);
  xb_rlock_unlock(&lock);
}

int main(void) {
  xb_platform_init();
  xb_thread threads[THREADS];
  for (size_t k = 0; k < THREADS; ++k) CHECK(xb_thread_start(&threads[k], 0, work, NULL));
  for (size_t k = 0; k < THREADS; ++k) xb_thread_join(threads[k]);
  CHECK(overlaps == 0 && total == THREADS * ROUNDS);

  xb_rlock_lock(&lock);
  xb_rlock_lock(&lock);
  xb_atomic_store32(&held, 1, XB_RELEASE);
  xb_thread t;
  CHECK(xb_thread_start(&t, 0, probe, NULL));
  xb_platform_sleep_ms(20);
  xb_rlock_unlock(&lock); /* still held once */
  xb_platform_sleep_ms(20);
  xb_atomic_store32(&held, 0, XB_RELEASE);
  xb_rlock_unlock(&lock);
  xb_thread_join(t);
  CHECK(probed);
  CHECK(lock.owner == 0 && lock.depth == 0);

  uint64_t self = xb_thread_self();
  uint32_t state = 1, depth = 1;
  uint64_t other = self + ((uint64_t)1 << 32);
  xb_rwords w = {&state, &other, &depth};
  CHECK(!xb_rwords_try(w, self) && depth == 1);
  CHECK(!xb_rwords_release(w, self) && state == 1 && other == self + ((uint64_t)1 << 32));
  CHECK(xb_rwords_release(w, other) && state == 0 && other == 0);
  CHECK(xb_flow_self() == self); /* no flow entered: the thread's own */
  xb_flow f = {0, NULL};
  xb_flow_enter(&f);
  uint64_t n = xb_flow_self();
  CHECK(n != self && n != 0 && f.number == n && xb_flow_outer(self) && !xb_flow_outer(n));
  xb_flow_leave(&f);
  puts("rlock ok");
  return 0;
}
