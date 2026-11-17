/* The most workers the scheduler starts: their stacks (XB_WORKER_STACK each) in a quarter of the address space the
 * process may map. On POSIX systems, with the limit lowered before the pool is made (RLIMIT_AS: 2 GiB, 1 GiB for
 * 32-bit pointers, when it was higher), the most is that quarter's worth of stacks; on any system it is at least one,
 * at most 1024, at least the number of workers that run at once, and no worker is alive before a task. */
#include "check.h"
#include "core/sched.h"
#include "platform/platform.h"

#include <stdint.h>

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

int main(void) {
  xb_platform_init();
  size_t expected = 0; /* the most, when the test set the limit */
#if !defined(_WIN32)
  struct rlimit was;
  CHECK(getrlimit(RLIMIT_AS, &was) == 0);
  rlim_t lower = (rlim_t)1 << 30;
  if (sizeof(void *) >= 8) lower *= 2;
  if (was.rlim_cur == RLIM_INFINITY || was.rlim_cur > lower) {
    struct rlimit set = was;
    set.rlim_cur = lower;
    CHECK(setrlimit(RLIMIT_AS, &set) == 0);
    expected = (size_t)lower / 4 / XB_WORKER_STACK;
  }
#endif
  uint32_t alive, runAtOnce, most;
  xb_sched_workers(&alive, &runAtOnce, &most);
  CHECK(alive == 0);
  CHECK(most >= 1 && most <= 1024 && runAtOnce >= 1 && runAtOnce <= most);
  if (expected) CHECK(most == expected);
  puts(expected ? "workers_most ok" : "workers_most ok (limited already)");
  return 0;
}
