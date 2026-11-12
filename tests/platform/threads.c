/* Threads, mutexes and the thread numbers: increments made by many threads at once, under a mutex and atomically,
 * all add up; a thread's stack is as large as asked; every thread has a number of its own. */
#include "check.h"
#include "core/sync.h"

#define THREADS 8
#define ROUNDS 200000u

static xb_mutex lock;
static uint32_t locked, atomic;
static uint32_t numbers[THREADS];

static void count(void *arg) {
  size_t k = (size_t)arg;
  numbers[k] = xb_thread_self();
  for (uint32_t i = 0; i < ROUNDS; ++i) {
    xb_mutex_lock(&lock);
    ++locked;
    xb_mutex_unlock(&lock);
    xb_atomic_add32(&atomic, 1, XB_RELAXED);
  }
}

/* About 12 MB of stack in frames of 4 KB: more than any system gives a thread by default. */
static uint32_t deep(uint32_t n) {
  volatile uint8_t frame[4096];
  frame[n % sizeof frame] = 1;
  return n ? deep(n - 1) + frame[n % sizeof frame] : 0;
}

static uint32_t depth;
static void recurse(void *arg) {
  (void)arg;
  depth = deep(3000);
}

static int tried;
static void try_held(void *arg) {
  (void)arg;
  tried = xb_mutex_trylock(&lock);
}

int main(void) {
  xb_platform_init();
  CHECK(xb_platform_cpu_count() >= 1);
  CHECK(xb_mutex_init(&lock));
  xb_thread threads[THREADS];
  for (size_t k = 0; k < THREADS; ++k) CHECK(xb_thread_start(&threads[k], 0, count, (void *)k));
  for (size_t k = 0; k < THREADS; ++k) xb_thread_join(threads[k]);
  CHECK(locked == THREADS * ROUNDS);
  CHECK(atomic == THREADS * ROUNDS);
  uint32_t self = xb_thread_self();
  CHECK(self != 0 && self == xb_thread_self());
  for (size_t k = 0; k < THREADS; ++k) {
    CHECK(numbers[k] != 0 && numbers[k] != self);
    for (size_t j = 0; j < k; ++j) CHECK(numbers[j] != numbers[k]);
  }

  xb_thread t;
  CHECK(xb_thread_start(&t, (size_t)16 << 20, recurse, NULL));
  xb_thread_join(t);
  CHECK(depth == 3000);

  xb_mutex_lock(&lock);
  CHECK(xb_thread_start(&t, 0, try_held, NULL));
  xb_thread_join(t);
  CHECK(!tried);
  xb_mutex_unlock(&lock);
  CHECK(xb_mutex_trylock(&lock));
  xb_mutex_unlock(&lock);
  xb_mutex_destroy(&lock);

  int64_t before = xb_platform_monotonic_ns();
  xb_platform_sleep_ms(5);
  CHECK(xb_platform_monotonic_ns() - before >= 4000000);
  puts("threads ok");
  return 0;
}
