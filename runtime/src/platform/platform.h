/* Operating-system services. Each platform directory implements this
 * interface completely (posix/, win32/); nothing outside platform/ uses OS
 * APIs directly. */
#ifndef XB_PLATFORM_H
#define XB_PLATFORM_H

#include <stddef.h>

#if defined(_MSC_VER)
#define XB_THREAD_LOCAL __declspec(thread)
#else
#define XB_THREAD_LOCAL _Thread_local
#endif

/* Locks. */
typedef struct xb_mutex_impl *xb_mutex;
void xb_mutex_init(xb_mutex *m);
void xb_mutex_lock(xb_mutex *m);
void xb_mutex_unlock(xb_mutex *m);

/* One-time process setup. */
void xb_platform_init(void);

/* Runs fn(arg) on a new thread whose stack can grow to `stack_size` bytes
 * (reserved, committed as it is used) and waits for it; returns 0 when the
 * thread could not be created. */
int xb_platform_run_on_stack(size_t stack_size, void (*fn)(void *), void *arg);

/* Today's date in local time. */
void xb_platform_local_date(int *year, int *month, int *day);

#endif
