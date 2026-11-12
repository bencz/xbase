/* Atomic operations on ordinary integers and pointers, so that the same memory may also be read and written plainly
 * where no other thread can see it (an object's header before it is shared, a field compiled code reads).
 *
 * A 64-bit field used atomically is aligned to 8 bytes (_Alignas), which 32-bit targets do not do by themselves.
 *
 * Orders: XB_RELAXED (atomicity only), XB_ACQUIRE (loads: nothing after it is done before it), XB_RELEASE (stores:
 * nothing before it is done after it), XB_ACQ_REL (read-modify-write: both), XB_SEQ_CST (one order every thread
 * agrees on). The read-modify-write operations return the value before them; xb_atomic_cas* replace `*expected` with
 * `desired` when the value is `*expected` (1), and otherwise store the value found in `*expected` (0).
 *
 * GCC and Clang use their __atomic builtins (a target without native atomics of some size links libatomic,
 * runtime/CMakeLists.txt). MSVC builds the runtime with /volatile:ms, which gives volatile loads acquire and volatile
 * stores release semantics on every architecture, and its Interlocked intrinsics (without a suffix: full barriers on
 * every architecture) for the rest. */
#ifndef XB_ATOMIC_H
#define XB_ATOMIC_H

#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER) && !defined(__clang__)

#include <intrin.h>

#if defined(_ISO_VOLATILE)
#error "the runtime is built with /volatile:ms (runtime/CMakeLists.txt): volatile accesses then acquire and release"
#endif

#define XB_RELAXED 0
#define XB_ACQUIRE 2
#define XB_RELEASE 3
#define XB_ACQ_REL 4
#define XB_SEQ_CST 5

#define XB_ATOMIC_UNUSED(order) ((void)(order))

static inline uint8_t xb_atomic_load8(const uint8_t *p, int order) {
  XB_ATOMIC_UNUSED(order);
  return *(const volatile uint8_t *)p;
}
static inline uint32_t xb_atomic_load32(const uint32_t *p, int order) {
  XB_ATOMIC_UNUSED(order);
  return *(const volatile uint32_t *)p;
}
static inline size_t xb_atomic_loadsz(const size_t *p, int order) {
  XB_ATOMIC_UNUSED(order);
  return *(const volatile size_t *)p;
}
static inline void *xb_atomic_loadptr(void *const *p, int order) {
  XB_ATOMIC_UNUSED(order);
  return *(void *const volatile *)p;
}

static inline void xb_atomic_store8(uint8_t *p, uint8_t v, int order) {
  if (order == XB_SEQ_CST) _InterlockedExchange8((volatile char *)p, (char)v);
  else *(volatile uint8_t *)p = v;
}
static inline void xb_atomic_store32(uint32_t *p, uint32_t v, int order) {
  if (order == XB_SEQ_CST) _InterlockedExchange((volatile long *)p, (long)v);
  else *(volatile uint32_t *)p = v;
}
static inline void xb_atomic_storesz(size_t *p, size_t v, int order) {
#if defined(_WIN64)
  if (order == XB_SEQ_CST) _InterlockedExchange64((volatile __int64 *)p, (__int64)v);
#else
  if (order == XB_SEQ_CST) _InterlockedExchange((volatile long *)p, (long)v);
#endif
  else *(volatile size_t *)p = v;
}
static inline void xb_atomic_storeptr(void **p, void *v, int order) {
  if (order == XB_SEQ_CST) _InterlockedExchangePointer((void *volatile *)p, v);
  else *(void *volatile *)p = v;
}

static inline uint8_t xb_atomic_or8(uint8_t *p, uint8_t v, int order) {
  XB_ATOMIC_UNUSED(order);
  return (uint8_t)_InterlockedOr8((volatile char *)p, (char)v);
}
static inline uint8_t xb_atomic_and8(uint8_t *p, uint8_t v, int order) {
  XB_ATOMIC_UNUSED(order);
  return (uint8_t)_InterlockedAnd8((volatile char *)p, (char)v);
}
static inline uint32_t xb_atomic_add32(uint32_t *p, uint32_t v, int order) {
  XB_ATOMIC_UNUSED(order);
  return (uint32_t)_InterlockedExchangeAdd((volatile long *)p, (long)v);
}
static inline uint32_t xb_atomic_or32(uint32_t *p, uint32_t v, int order) {
  XB_ATOMIC_UNUSED(order);
  return (uint32_t)_InterlockedOr((volatile long *)p, (long)v);
}
static inline uint32_t xb_atomic_and32(uint32_t *p, uint32_t v, int order) {
  XB_ATOMIC_UNUSED(order);
  return (uint32_t)_InterlockedAnd((volatile long *)p, (long)v);
}
static inline uint32_t xb_atomic_xchg32(uint32_t *p, uint32_t v, int order) {
  XB_ATOMIC_UNUSED(order);
  return (uint32_t)_InterlockedExchange((volatile long *)p, (long)v);
}
static inline int xb_atomic_cas32(uint32_t *p, uint32_t *expected, uint32_t desired, int order) {
  XB_ATOMIC_UNUSED(order);
  uint32_t found = (uint32_t)_InterlockedCompareExchange((volatile long *)p, (long)desired, (long)*expected);
  if (found == *expected) return 1;
  *expected = found;
  return 0;
}
static inline size_t xb_atomic_addsz(size_t *p, size_t v, int order) {
  XB_ATOMIC_UNUSED(order);
#if defined(_WIN64)
  return (size_t)_InterlockedExchangeAdd64((volatile __int64 *)p, (__int64)v);
#else
  return (size_t)_InterlockedExchangeAdd((volatile long *)p, (long)v);
#endif
}
static inline int xb_atomic_cassz(size_t *p, size_t *expected, size_t desired, int order) {
  XB_ATOMIC_UNUSED(order);
#if defined(_WIN64)
  size_t found = (size_t)_InterlockedCompareExchange64((volatile __int64 *)p, (__int64)desired, (__int64)*expected);
#else
  size_t found = (size_t)_InterlockedCompareExchange((volatile long *)p, (long)desired, (long)*expected);
#endif
  if (found == *expected) return 1;
  *expected = found;
  return 0;
}
static inline void *xb_atomic_xchgptr(void **p, void *v, int order) {
  XB_ATOMIC_UNUSED(order);
  return _InterlockedExchangePointer((void *volatile *)p, v);
}
static inline int xb_atomic_casptr(void **p, void **expected, void *desired, int order) {
  XB_ATOMIC_UNUSED(order);
  void *found = _InterlockedCompareExchangePointer((void *volatile *)p, desired, *expected);
  if (found == *expected) return 1;
  *expected = found;
  return 0;
}

/* 64-bit loads and stores of a 32-bit target are no single access: they go through compare-and-swap there. */
static inline uint64_t xb_atomic_load64(const uint64_t *p, int order) {
  XB_ATOMIC_UNUSED(order);
#if defined(_WIN64)
  return *(const volatile uint64_t *)p;
#else
  return (uint64_t)_InterlockedCompareExchange64((volatile __int64 *)p, 0, 0);
#endif
}
static inline void xb_atomic_store64(uint64_t *p, uint64_t v, int order) {
#if defined(_WIN64)
  if (order != XB_SEQ_CST) {
    *(volatile uint64_t *)p = v;
    return;
  }
#endif
  XB_ATOMIC_UNUSED(order);
  _InterlockedExchange64((volatile __int64 *)p, (__int64)v);
}
static inline uint64_t xb_atomic_add64(uint64_t *p, uint64_t v, int order) {
  XB_ATOMIC_UNUSED(order);
  return (uint64_t)_InterlockedExchangeAdd64((volatile __int64 *)p, (__int64)v);
}

static inline void xb_atomic_fence(int order) {
  volatile long barrier = 0;
  XB_ATOMIC_UNUSED(order);
  _InterlockedOr(&barrier, 0);
}

#else

#define XB_RELAXED __ATOMIC_RELAXED
#define XB_ACQUIRE __ATOMIC_ACQUIRE
#define XB_RELEASE __ATOMIC_RELEASE
#define XB_ACQ_REL __ATOMIC_ACQ_REL
#define XB_SEQ_CST __ATOMIC_SEQ_CST

/* The order of a failed compare-and-swap, which only loads. */
#define XB_ATOMIC_FAIL_ORDER(order) \
  ((order) == XB_ACQ_REL ? XB_ACQUIRE : (order) == XB_RELEASE ? XB_RELAXED : (order))

static inline uint8_t xb_atomic_load8(const uint8_t *p, int order) { return __atomic_load_n(p, order); }
static inline uint32_t xb_atomic_load32(const uint32_t *p, int order) { return __atomic_load_n(p, order); }
static inline size_t xb_atomic_loadsz(const size_t *p, int order) { return __atomic_load_n(p, order); }
static inline void *xb_atomic_loadptr(void *const *p, int order) { return __atomic_load_n(p, order); }

static inline void xb_atomic_store8(uint8_t *p, uint8_t v, int order) { __atomic_store_n(p, v, order); }
static inline void xb_atomic_store32(uint32_t *p, uint32_t v, int order) { __atomic_store_n(p, v, order); }
static inline void xb_atomic_storesz(size_t *p, size_t v, int order) { __atomic_store_n(p, v, order); }
static inline void xb_atomic_storeptr(void **p, void *v, int order) { __atomic_store_n(p, v, order); }

static inline uint8_t xb_atomic_or8(uint8_t *p, uint8_t v, int order) { return __atomic_fetch_or(p, v, order); }
static inline uint8_t xb_atomic_and8(uint8_t *p, uint8_t v, int order) { return __atomic_fetch_and(p, v, order); }
static inline uint32_t xb_atomic_add32(uint32_t *p, uint32_t v, int order) { return __atomic_fetch_add(p, v, order); }
static inline uint32_t xb_atomic_or32(uint32_t *p, uint32_t v, int order) { return __atomic_fetch_or(p, v, order); }
static inline uint32_t xb_atomic_and32(uint32_t *p, uint32_t v, int order) { return __atomic_fetch_and(p, v, order); }
static inline uint32_t xb_atomic_xchg32(uint32_t *p, uint32_t v, int order) { return __atomic_exchange_n(p, v, order); }
static inline int xb_atomic_cas32(uint32_t *p, uint32_t *expected, uint32_t desired, int order) {
  return __atomic_compare_exchange_n(p, expected, desired, 0, order, XB_ATOMIC_FAIL_ORDER(order));
}
static inline size_t xb_atomic_addsz(size_t *p, size_t v, int order) { return __atomic_fetch_add(p, v, order); }
static inline int xb_atomic_cassz(size_t *p, size_t *expected, size_t desired, int order) {
  return __atomic_compare_exchange_n(p, expected, desired, 0, order, XB_ATOMIC_FAIL_ORDER(order));
}
static inline void *xb_atomic_xchgptr(void **p, void *v, int order) { return __atomic_exchange_n(p, v, order); }
static inline int xb_atomic_casptr(void **p, void **expected, void *desired, int order) {
  return __atomic_compare_exchange_n(p, expected, desired, 0, order, XB_ATOMIC_FAIL_ORDER(order));
}

static inline uint64_t xb_atomic_load64(const uint64_t *p, int order) { return __atomic_load_n(p, order); }
static inline void xb_atomic_store64(uint64_t *p, uint64_t v, int order) { __atomic_store_n(p, v, order); }
static inline uint64_t xb_atomic_add64(uint64_t *p, uint64_t v, int order) { return __atomic_fetch_add(p, v, order); }

static inline void xb_atomic_fence(int order) { __atomic_thread_fence(order); }

#endif

#endif
