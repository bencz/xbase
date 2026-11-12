/* xb_once: many threads asking at once run the function exactly once, and every one of them returns only after it
 * has finished (it sees what the function wrote). */
#include "check.h"
#include "core/sync.h"

#define THREADS 16

static xb_once_flag flag = XB_ONCE_INIT;
static uint32_t runs, ready, start, sawReady;

static void init(void) {
  xb_atomic_add32(&runs, 1, XB_RELAXED);
  xb_platform_sleep_ms(20); /* the others arrive meanwhile and wait */
  ready = 1;
}

static void ask(void *arg) {
  (void)arg;
  while (!xb_atomic_load32(&start, XB_ACQUIRE)) xb_platform_sleep_ms(0);
  xb_once(&flag, init);
  if (ready) xb_atomic_add32(&sawReady, 1, XB_RELAXED);
}

int main(void) {
  xb_platform_init();
  xb_thread threads[THREADS];
  for (size_t k = 0; k < THREADS; ++k) CHECK(xb_thread_start(&threads[k], 0, ask, NULL));
  xb_atomic_store32(&start, 1, XB_RELEASE);
  for (size_t k = 0; k < THREADS; ++k) xb_thread_join(threads[k]);
  CHECK(runs == 1 && sawReady == THREADS);
  xb_once(&flag, init);
  CHECK(runs == 1);
  puts("once ok");
  return 0;
}
