/* Reader-writer locks: writers keep two numbers equal, changing them one after the other; readers, many at once,
 * never see them differ. */
#include "check.h"
#include "core/sync.h"

#define READERS 8
#define WRITERS 2
#define WRITES 20000u

static xb_rwlock lock;
static volatile uint32_t first, second;
static uint32_t writersLeft = WRITERS, mismatches, reads;

static void write_pairs(void *arg) {
  (void)arg;
  for (uint32_t i = 0; i < WRITES; ++i) {
    xb_rwlock_write_lock(&lock);
    first = first + 1;
    second = second + 1;
    xb_rwlock_write_unlock(&lock);
  }
  xb_atomic_add32(&writersLeft, (uint32_t)-1, XB_RELEASE);
}

static void read_pairs(void *arg) {
  (void)arg;
  while (xb_atomic_load32(&writersLeft, XB_ACQUIRE)) {
    xb_rwlock_read_lock(&lock);
    if (first != second) xb_atomic_add32(&mismatches, 1, XB_RELAXED);
    xb_rwlock_read_unlock(&lock);
    xb_atomic_add32(&reads, 1, XB_RELAXED);
  }
}

int main(void) {
  xb_platform_init();
  CHECK(xb_rwlock_init(&lock));
  xb_thread threads[READERS + WRITERS];
  for (size_t k = 0; k < READERS + WRITERS; ++k)
    CHECK(xb_thread_start(&threads[k], 0, k < WRITERS ? write_pairs : read_pairs, NULL));
  for (size_t k = 0; k < READERS + WRITERS; ++k) xb_thread_join(threads[k]);
  CHECK(mismatches == 0);
  CHECK(first == WRITERS * WRITES && second == WRITERS * WRITES);
  xb_rwlock_destroy(&lock);
  puts("rwlock ok");
  return 0;
}
