/* Synchronization built on the platform's primitives (platform.h, atomic.h), the same on every system. */
#ifndef XB_SYNC_H
#define XB_SYNC_H

#include "platform/atomic.h"
#include "platform/platform.h"

#include <stdint.h>

/* A number for the calling thread: never 0, and no other thread's while fewer than 2^32 threads have asked. */
uint32_t xb_thread_self(void);

/* Runs fn() once for the flag, whichever threads call it at once: the others wait until it has returned. fn never
 * calls xb_once with its own flag. A flag starts as XB_ONCE_INIT, statically. */
typedef struct xb_once_flag {
  uint32_t state;
} xb_once_flag;
#define XB_ONCE_INIT {0}
void xb_once(xb_once_flag *flag, void (*fn)(void));

/* A lock the thread holding it may take again, released when it has been released as many times as taken (a release
 * by another thread ends the process: a defect of the runtime); made statically as XB_RLOCK_INIT, it needs no setup.
 * For rare, nesting work (preparing classes), not hot paths. */
typedef struct xb_rlock {
  uint32_t owner; /* the holder's xb_thread_self, 0 when free */
  uint32_t depth;
} xb_rlock;
#define XB_RLOCK_INIT {0, 0}
void xb_rlock_lock(xb_rlock *l);
void xb_rlock_unlock(xb_rlock *l);

/* Waiting on an address (a parking lot): xb_park_wait waits while the 32-bit value at `addr` is `expected`, until a
 * xb_park_wake on the same address, or `ns` nanoseconds of the monotonic clock (ns < 0: no limit); 0 when it timed out.
 * The value is tested under the lock xb_park_wake takes, so a thread that changes it and then wakes the address never
 * misses a waiter. A wait may also end for no reason: the waiter tests its condition again. xb_park_wake wakes the
 * waiter of the address that came first (`all`: every one) and returns how many it woke. */
int xb_park_wait(const uint32_t *addr, uint32_t expected, int64_t ns);
uint32_t xb_park_wake(const uint32_t *addr, int all);

#endif
