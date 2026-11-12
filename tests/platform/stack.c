/* The lowest usable address of a thread's stack: below the thread's own variables, on the process's first thread and
 * on one started with a 16 MB stack, where it lies about 16 MB below them. */
#include "check.h"
#include "core/sync.h"

#define SIZE ((size_t)16 << 20)

static ptrdiff_t room;
static int known;

static void measure(void *arg) {
  (void)arg;
  volatile char here = 0;
  const char *low = xb_platform_stack_low();
  known = low != NULL;
  if (known) room = (const char *)&here - low;
}

int main(void) {
  xb_platform_init();
  volatile char here = 0;
  const char *low = xb_platform_stack_low();
  if (!low) { /* a system that does not tell: the stack error is then never raised */
    puts("stack ok (unknown)");
    return 0;
  }
  CHECK(low < (const char *)&here);
  xb_thread t;
  CHECK(xb_thread_start(&t, SIZE, measure, NULL));
  xb_thread_join(t);
  CHECK(known);
  CHECK(room > (ptrdiff_t)(SIZE - ((size_t)1 << 20)) && room <= (ptrdiff_t)SIZE);
  puts("stack ok");
  return 0;
}
