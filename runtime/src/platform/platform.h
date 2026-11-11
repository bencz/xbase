/* Operating-system services. Each platform directory implements this
 * interface completely (posix/, win32/); nothing outside platform/ uses OS
 * APIs directly. */
#ifndef XB_PLATFORM_H
#define XB_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER)
#define XB_THREAD_LOCAL __declspec(thread)
#else
#define XB_THREAD_LOCAL _Thread_local
#endif

/* Locks. */
typedef struct xb_mutex_impl *xb_mutex;
int xb_mutex_init(xb_mutex *m); /* 0: no memory or no lock available */
void xb_mutex_lock(xb_mutex *m);
void xb_mutex_unlock(xb_mutex *m);

/* One-time process setup; it also starts the clock of xb_platform_elapsed_ms. */
void xb_platform_init(void);

/* Milliseconds since xb_platform_init, from a monotonic clock (never set back,
 * unaffected by changes of the date and time). */
int64_t xb_platform_elapsed_ms(void);

/* Runs fn(arg) on a new thread whose stack can grow to `stack_size` bytes
 * (reserved, committed as it is used) and waits for it; returns 0 when the
 * thread could not be created. */
int xb_platform_run_on_stack(size_t stack_size, void (*fn)(void *), void *arg);

/* The program's command-line arguments in UTF-8 (argv[0], the program, first):
 * the array main() received where it already is UTF-8, else a converted copy
 * that lives as long as the process. */
char **xb_platform_args(int argc, char **argv, int *count);

/* Today's date in local time. */
void xb_platform_local_date(int *year, int *month, int *day);

/* The local time of day, to the millisecond. */
void xb_platform_local_time(int *hour, int *minute, int *second, int *msec);

/* Whether `c` separates the directories of a path ('/', and on Windows '\\'
 * and the ':' after a drive letter). */
int xb_platform_path_separator(char c);

/* Files. A file is the system's handle as an integer; `path` is UTF-8. A
 * function that fails returns 0 (a count: -1) and stores in `*err` the code
 * that FError() reports: a DOS error number, which Windows reports itself and
 * the POSIX errno values are translated to (2 file not found, 3 path not
 * found, 4 too many open files, 5 access denied, 6 invalid handle, 8 not
 * enough memory, 32 sharing violation, 80 file exists, 87 invalid parameter,
 * 112 disk full, 206 name too long; 31, general failure, for the others). */
typedef intptr_t xb_file;

/* Opens an existing file for reading. */
int xb_platform_file_open(const char *path, xb_file *f, int32_t *err);

/* Creates a file for writing, emptying it when it exists. */
int xb_platform_file_create(const char *path, xb_file *f, int32_t *err);

/* Reads up to `n` bytes: the count read, 0 at the end of the file. */
int64_t xb_platform_file_read(xb_file f, void *buf, size_t n, int32_t *err);

/* Writes the `n` bytes: 1 when all of them were written. */
int xb_platform_file_write(xb_file f, const void *buf, size_t n, int32_t *err);

/* Closes the file: 1 when nothing failed. */
int xb_platform_file_close(xb_file f, int32_t *err);

#endif
