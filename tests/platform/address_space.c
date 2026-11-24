/* The address space the system lets the process map (the scheduler's workers keep their stacks in a quarter of it): on
 * POSIX systems a limit lowered on the process (RLIMIT_AS, set to 64 GiB, or 1 GiB for 32-bit pointers, when it was
 * higher) is what xb_platform_address_space tells, and SIZE_MAX with none or with one past what size_t holds; on
 * Windows, the process's virtual space, at least 1 GiB. */
/* 64-bit limits on 32-bit systems too, as the runtime reads them: a limit past 32 bits (a hard one set outside) then
 * reads as itself, and is written back as itself. */
#define _FILE_OFFSET_BITS 64
#include "check.h"
#include "platform/platform.h"

#include <stdint.h>

#if defined(_WIN32)
int main(void) {
  xb_platform_init();
  size_t space = xb_platform_address_space();
  CHECK(space >= ((size_t)1 << 30));
  puts("address_space ok");
  return 0;
}
#else
#include <sys/resource.h>

/* More than pointers reach (a limit of a 64-bit rlim_t past a 32-bit size_t). */
static int beyond_pointers(uint64_t limit) { return limit > SIZE_MAX; }

int main(void) {
  xb_platform_init();
  struct rlimit was;
  CHECK(getrlimit(RLIMIT_AS, &was) == 0);
  int none = was.rlim_cur == RLIM_INFINITY || beyond_pointers((uint64_t)was.rlim_cur);
  CHECK(xb_platform_address_space() == (none ? SIZE_MAX : (size_t)was.rlim_cur));
  rlim_t lower = (rlim_t)1 << 30;
  if (sizeof(void *) >= 8 && sizeof(rlim_t) >= 8) lower <<= 6;
  if (was.rlim_cur != RLIM_INFINITY && was.rlim_cur <= lower) {
    puts("address_space ok (limited already)");
    return 0;
  }
  struct rlimit set = was;
  set.rlim_cur = lower;
  CHECK(setrlimit(RLIMIT_AS, &set) == 0);
  CHECK(xb_platform_address_space() == (size_t)lower);
  CHECK(setrlimit(RLIMIT_AS, &was) == 0);
  puts("address_space ok");
  return 0;
}
#endif
