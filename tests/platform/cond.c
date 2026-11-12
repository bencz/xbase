/* Condition variables: producers and consumers through a small queue (every item arrives once), and timed waits
 * nobody signals: each one that reports a timeout took at least the time asked, and they do time out. */
#include "check.h"
#include "core/sync.h"

#define PRODUCERS 4
#define CONSUMERS 4
#define ITEMS 20000u
#define SLOTS 8u

static xb_mutex lock;
static xb_cond notEmpty, notFull;
static uint32_t queue[SLOTS], head, count, produced, consumed;
static uint64_t sum;

static void produce(void *arg) {
  (void)arg;
  for (;;) {
    xb_mutex_lock(&lock);
    while (count == SLOTS && produced < PRODUCERS * ITEMS) xb_cond_wait(&notFull, &lock);
    if (produced == PRODUCERS * ITEMS) {
      xb_mutex_unlock(&lock);
      return;
    }
    queue[(head + count) % SLOTS] = ++produced;
    ++count;
    xb_cond_signal(&notEmpty);
    xb_mutex_unlock(&lock);
  }
}

static void consume(void *arg) {
  (void)arg;
  for (;;) {
    xb_mutex_lock(&lock);
    while (count == 0 && consumed < PRODUCERS * ITEMS) xb_cond_wait(&notEmpty, &lock);
    if (consumed == PRODUCERS * ITEMS) {
      xb_cond_broadcast(&notEmpty); /* the other consumers end too, and producers still waiting */
      xb_cond_broadcast(&notFull);
      xb_mutex_unlock(&lock);
      return;
    }
    sum += queue[head];
    head = (head + 1) % SLOTS;
    --count;
    ++consumed;
    xb_cond_signal(&notFull);
    xb_mutex_unlock(&lock);
  }
}

int main(void) {
  xb_platform_init();
  CHECK(xb_mutex_init(&lock) && xb_cond_init(&notEmpty) && xb_cond_init(&notFull));
  xb_thread threads[PRODUCERS + CONSUMERS];
  for (size_t k = 0; k < PRODUCERS + CONSUMERS; ++k)
    CHECK(xb_thread_start(&threads[k], 0, k < PRODUCERS ? produce : consume, NULL));
  for (size_t k = 0; k < PRODUCERS + CONSUMERS; ++k) xb_thread_join(threads[k]);
  uint64_t n = (uint64_t)PRODUCERS * ITEMS;
  CHECK(consumed == n && sum == n * (n + 1) / 2);

  xb_mutex_lock(&lock);
  int timeouts = 0; /* a wait may end early for no reason, but not every one of a hundred */
  for (int i = 0; i < 100 && timeouts < 3; ++i) {
    int64_t before = xb_platform_monotonic_ns();
    if (!xb_cond_wait_ns(&notEmpty, &lock, 5 * XB_NS_PER_MS)) {
      CHECK(xb_platform_monotonic_ns() - before >= 5 * XB_NS_PER_MS);
      ++timeouts;
    }
  }
  CHECK(timeouts == 3);
  xb_mutex_unlock(&lock);
  xb_cond_destroy(&notEmpty);
  xb_cond_destroy(&notFull);
  xb_mutex_destroy(&lock);
  puts("cond ok");
  return 0;
}
