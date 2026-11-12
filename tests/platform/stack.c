/* A thread's stack as the system reserved it: its variables lie between the lowest usable address and the end, on the
 * process's first thread and on one started with a 16 MB stack, whose lowest address lies about 16 MB below them. */
#include "check.h"
#include "core/sync.h"

#define SIZE ((size_t)16 << 20)

static ptrdiff_t room;
static int known, inside;

static void measure(void *arg) {
  (void)arg;
  volatile char here = 0;
  const char *low, *high;
  known = xb_platform_stack_bounds(&low, &high);
  if (!known) return;
  room = (const char *)&here - low;
  inside = low < (const char *)&here && (const char *)&here < high && high - low <= (ptrdiff_t)(SIZE + (1u << 20));
}

int main(void) {
  xb_platform_init();
  volatile char here = 0;
  const char *low, *high;
  if (!xb_platform_stack_bounds(&low, &high)) { /* a system that does not tell: the stack error is never raised */
    CHECK(!low && !high);
    puts("stack ok (unknown)");
    return 0;
  }
  CHECK(low < (const char *)&here && (const char *)&here < high);
  xb_thread t;
  CHECK(xb_thread_start(&t, SIZE, measure, NULL));
  xb_thread_join(t);
  CHECK(known && inside);
  CHECK(room > (ptrdiff_t)(SIZE - ((size_t)1 << 20)) && room <= (ptrdiff_t)SIZE);
  puts("stack ok");
  return 0;
}
