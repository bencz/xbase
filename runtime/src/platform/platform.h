/* Operating-system services. Each platform directory implements this
 * interface completely (posix/, win32/); nothing outside platform/ uses OS
 * APIs directly. */
#ifndef XB_PLATFORM_H
#define XB_PLATFORM_H

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

#endif
