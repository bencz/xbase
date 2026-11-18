/* Detached threads that go on running a while after they are let go: on Windows xb_platform_threads_wait returns once
 * every one has ended at the system's level: each has run to its end, and the process has its first thread alone
 * (counted in a snapshot of the system's threads), more than once (the threads detached later are waited for too).
 * Elsewhere it waits for nothing (the process's end ends them, and keeps its exit code): the test waits for them. */
#include "check.h"
#include "core/sync.h"

#if defined(_WIN32)
#include <windows.h>

#include <tlhelp32.h>
#endif

#define THREADS 6

static uint32_t go, ran;

static void linger(void *arg) {
  (void)arg;
  while (!xb_atomic_load32(&go, XB_ACQUIRE)) xb_thread_yield();
  int64_t until = xb_platform_monotonic_ns() + 30 * 1000 * 1000; /* still running after the waiter starts waiting */
  while (xb_platform_monotonic_ns() < until) {
  }
  xb_atomic_add32(&ran, 1, XB_RELEASE);
}

#if defined(_WIN32)
/* The threads of the process now. */
static unsigned threads_now(void) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  CHECK(snap != INVALID_HANDLE_VALUE);
  THREADENTRY32 e;
  e.dwSize = sizeof e;
  unsigned n = 0;
  for (BOOL more = Thread32First(snap, &e); more; more = Thread32Next(snap, &e))
    if (e.th32OwnerProcessID == GetCurrentProcessId()) ++n;
  CloseHandle(snap);
  return n;
}
#endif

static void round_of(uint32_t n) {
  xb_atomic_store32(&go, 0, XB_RELEASE);
  for (uint32_t k = 0; k < n; ++k) {
    xb_thread t;
    CHECK(xb_thread_start(&t, 0, linger, NULL));
    xb_thread_detach(t);
  }
  uint32_t before = xb_atomic_load32(&ran, XB_ACQUIRE);
  xb_atomic_store32(&go, 1, XB_RELEASE);
  xb_platform_threads_wait();
#if defined(_WIN32)
  CHECK(xb_atomic_load32(&ran, XB_ACQUIRE) == before + n);
  CHECK(threads_now() == 1);
#else
  int64_t limit = xb_platform_monotonic_ns() + (int64_t)10 * 1000 * 1000 * 1000;
  while (xb_atomic_load32(&ran, XB_ACQUIRE) != before + n && xb_platform_monotonic_ns() < limit) xb_thread_yield();
  CHECK(xb_atomic_load32(&ran, XB_ACQUIRE) == before + n);
#endif
}

int main(void) {
  xb_platform_init();
  round_of(THREADS);
  round_of(THREADS * 2);
  puts("threads_end ok");
  return 0;
}
