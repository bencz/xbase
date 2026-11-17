/* Synchronization built on the platform's primitives (platform.h, atomic.h), the same on every system. */
#ifndef XB_SYNC_H
#define XB_SYNC_H

#include "platform/atomic.h"
#include "platform/platform.h"

#include <stdint.h>

/* The span of memory that caches move between processors at once: words that different threads change often are
 * kept this far apart (a larger line only shares more; a smaller one is padded past). */
#define XB_CACHE_LINE 64

/* Owners of locks (xb_rwords): a 64-bit number never 0 and never given twice (one counter, which a process could not
 * run long enough to wrap).
 * xb_thread_self is the calling thread's own, for the runtime's locks, which no code of the program's runs under.
 * xb_flow_self is the flow's that runs on the thread, for the program's locks (a Mutex): the thread's own, or the
 * innermost flow entered on it, a task's or an asynchronous function's step (xb_flow_enter, xb_flow_leave: a record on
 * the C stack, `number` 0 until the flow first asks for it; a function's steps keep one number, so that a lock it holds
 * goes with it across AWAIT, from worker to worker). xb_flow_outer tells whether `owner` is a flow the running one runs
 * inside on this thread (the thread's, or one entered further out): a wait for a lock it holds would never end. */
typedef struct xb_flow {
  uint64_t number;
  struct xb_flow *outer;
} xb_flow;
uint64_t xb_thread_self(void);
uint64_t xb_flow_self(void);
void xb_flow_enter(xb_flow *flow);
void xb_flow_leave(xb_flow *flow);
int  xb_flow_outer(uint64_t owner);

/* Runs fn() once for the flag, whichever threads call it at once: the others wait until it has returned. fn never
 * calls xb_once with its own flag. A flag starts as XB_ONCE_INIT, statically. */
typedef struct xb_once_flag {
  uint32_t state;
} xb_once_flag;
#define XB_ONCE_INIT {0}
void xb_once(xb_once_flag *flag, void (*fn)(void));

/* A lock its holder may take again, released when it has been released as many times as taken, over three words: the
 * state (0 free, 1 held, 2 held and maybe waited for), the holder's number (`self`: xb_thread_self or xb_flow_self;
 * 0: none), how many times it took the lock (the holder's alone). xb_rlock keeps them together; a Mutex object keeps
 * them in its variables (rtl/threads.c), which waits of its own read again between waits. xb_rwords_try takes the lock
 * when `self` holds it already or finds it free: 1; 0 when another holds it. xb_rwords_claim, after a failed try,
 * marks the lock waited for and takes it if it was free meanwhile: 1; 0 when the caller must wait while the state is 2
 * (on its address) and claim again. xb_rwords_release releases it once (the next waiter woken when it is free): 0 when
 * `self` does not hold it. The holder's word is 64 bits, 8-aligned, read and written atomically: a torn read could
 * show another flow its own number. */
typedef struct xb_rwords {
  uint32_t *state;
  uint64_t *owner;
  uint32_t *depth;
} xb_rwords;
int xb_rwords_try(xb_rwords w, uint64_t self);
int xb_rwords_claim(xb_rwords w, uint64_t self);
int xb_rwords_release(xb_rwords w, uint64_t self);

/* The lock made statically as XB_RLOCK_INIT, which needs no setup, owned by the thread (a release by another thread
 * ends the process: a defect of the runtime). For rare, nesting work (preparing classes), not hot paths. */
typedef struct xb_rlock {
  _Alignas(8) uint64_t owner;
  uint32_t state, depth;
} xb_rlock;
#define XB_RLOCK_INIT {0, 0, 0}
void xb_rlock_lock(xb_rlock *l);
void xb_rlock_unlock(xb_rlock *l);

/* Waiting on an address (a parking lot): xb_park_wait waits while the 32-bit value at `addr` is `expected`, until a
 * xb_park_wake on the same address, or `ns` nanoseconds of the monotonic clock (ns < 0: no limit); 0 when it timed out.
 * A waiter counts itself in its bucket, then reads the value (both sequentially consistent); the waker, after it
 * changed the value, reads the count (after a full fence): when it is 0 the wake costs no lock, else the waiter's test
 * and the waker's wake are ordered by the bucket's lock, so a thread that changes the value and then wakes the address
 * never misses a waiter. A wait may also end for no reason: the waiter tests its condition again. xb_park_wake wakes
 * the waiter of the address that came first (`all`: every one) and returns how many it woke. */
int xb_park_wait(const uint32_t *addr, uint32_t expected, int64_t ns);
uint32_t xb_park_wake(const uint32_t *addr, int all);

/* xb_park_wait as a blocking operation (xb_blocking_run): collections go on without the waiter, which may wait for a
 * thread that waits for a collection. */
void xb_park_wait_blocking(const uint32_t *addr, uint32_t expected, int64_t ns);
/* The same, not sleeping either while the word at `stop` has a bit of `mask` (a context's pending QUIT): whoever sets
 * it and then wakes `addr` never misses the waiter. */
void xb_park_wait_blocking_stop(const uint32_t *addr, uint32_t expected, const uint32_t *stop, uint32_t mask,
                                int64_t ns);
/* The same for a short wait (xb_waiting_run): a lock of the runtime's, which its holder keeps a moment. */
void xb_park_wait_short(const uint32_t *addr, uint32_t expected, int64_t ns);

/* The lock of the standard streams: each write of the program (what one QOut() or OutStd() call shows, an error
 * report) holds it, so the outputs of threads never split one another. Taken inside a blocking operation. */
void xb_output_lock(void);
void xb_output_unlock(void);

#endif
