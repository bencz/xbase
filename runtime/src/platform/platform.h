/* Operating-system services. Each platform directory implements this
 * interface completely (posix/, win32/); nothing outside platform/ uses OS
 * APIs directly. */
#ifndef XB_PLATFORM_H
#define XB_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "fileio.ch"

/* XB_NOINLINE keeps a rare path out of a hot function, which then needs no
 * frame of its own. XB_SCANS_STACKS marks the function that reads other
 * threads' stacks word by word (the collector's conservative scan): what it
 * reads may be written meanwhile (a blocked thread's results and buffers, never
 * the addresses it scans for) and lies between the variables of frames, which
 * the address and thread sanitizers would report. */
#if defined(_MSC_VER)
#define XB_THREAD_LOCAL __declspec(thread)
#define XB_NOINLINE __declspec(noinline)
#else
#define XB_THREAD_LOCAL _Thread_local
#define XB_NOINLINE __attribute__((noinline))
#endif
#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 8)
#define XB_SCANS_STACKS __attribute__((no_sanitize("address", "thread")))
#else
#define XB_SCANS_STACKS
#endif

/* Synchronization between threads. Each kind is created by its init function (0: no memory or no system object
 * available; the handle is then NULL) and freed by its destroy function, which nothing may be waiting on. The lock
 * functions never fail. No lock is recursive: the thread holding one never asks for it again (not even a second read
 * lock of a reader-writer lock, which waits behind a waiting writer). */

#define XB_NS_PER_SEC INT64_C(1000000000)
#define XB_NS_PER_MS INT64_C(1000000)
#define XB_NS_PER_US INT64_C(1000)

/* A mutex. */
typedef struct xb_mutex_impl *xb_mutex;
int xb_mutex_init(xb_mutex *m);
void xb_mutex_destroy(xb_mutex *m);
void xb_mutex_lock(xb_mutex *m);
int xb_mutex_trylock(xb_mutex *m); /* 1: taken, without waiting */
void xb_mutex_unlock(xb_mutex *m);

/* A condition variable, waited on with a mutex the waiter holds, which is released while it waits and taken again
 * before the wait returns. A wait may also return without a signal: the waiter tests its condition again.
 * xb_cond_wait_ns gives up after `ns` nanoseconds of the monotonic clock: 0 when it timed out. */
typedef struct xb_cond_impl *xb_cond;
#define XB_WAIT_NS_MAX ((int64_t)1 << 60) /* a longer timed wait is this long (about 36 years) */
int xb_cond_init(xb_cond *c);
void xb_cond_destroy(xb_cond *c);
void xb_cond_wait(xb_cond *c, xb_mutex *m);
int xb_cond_wait_ns(xb_cond *c, xb_mutex *m, int64_t ns);
void xb_cond_signal(xb_cond *c);    /* wakes one waiter, if any */
void xb_cond_broadcast(xb_cond *c); /* wakes them all */

/* A reader-writer lock: many readers, or one writer. Where the system lets the kind be chosen (glibc), a waiting
 * writer goes before readers that come after it; elsewhere the system's own policy decides. */
typedef struct xb_rwlock_impl *xb_rwlock;
int xb_rwlock_init(xb_rwlock *l);
void xb_rwlock_destroy(xb_rwlock *l);
void xb_rwlock_read_lock(xb_rwlock *l);
void xb_rwlock_read_unlock(xb_rwlock *l);
void xb_rwlock_write_lock(xb_rwlock *l);
void xb_rwlock_write_unlock(xb_rwlock *l);

/* Runs fn(arg), which may block the calling thread (a file's I/O, a wait), so that a collection does not wait for it:
 * the runtime provides it (gc.c). fn allocates nothing and changes no reference; it may read and write objects the
 * thread keeps rooted (a buffer a read fills), which do not move meanwhile. Nested, it simply calls fn. A worker of the
 * scheduler that runs it keeps its place while fn returns soon, and gives it to another when fn blocks (the monitor
 * finds it there: XB_BLOCK_MAY, core/sched.h); xb_sleeping_run is the same for what waits (a park, a pause, a
 * command), whose worker gives its place at once; xb_waiting_run for a short wait (for a lock of the runtime's, held a
 * moment), which keeps the worker's place. */
void xb_blocking_run(void (*fn)(void *), void *arg);
void xb_sleeping_run(void (*fn)(void *), void *arg);
void xb_waiting_run(void (*fn)(void *), void *arg);

/* fflush(stream) as a blocking operation (a full pipe holds it): the standard output before the standard error is
 * written, the program's output at its end. */
void xb_platform_flush(FILE *stream);

/* Threads. xb_thread_start runs fn(arg) on a new thread whose stack can grow to `stack_size` bytes (reserved, committed
 * as it is used; rounded up to the system's page and minimum; 0: the system's default): 0 when it could not be
 * created. xb_thread_join waits for the thread to end
 * and frees it; xb_thread_detach lets it go, freed when it ends (it may have ended already): every thread started is
 * joined or detached once. xb_thread_yield lets another thread that is ready run on the caller's processor (a
 * waiter's back-off). */
typedef struct xb_thread_impl *xb_thread;
int xb_thread_start(xb_thread *t, size_t stack_size, void (*fn)(void *), void *arg);
void xb_thread_join(xb_thread t);
void xb_thread_detach(xb_thread t);
/* Waits until every thread detached so far has ended at the system's level (past its last instruction), for the
 * process to end alone: on Windows a thread that ends while the process exits can give the process its own exit code
 * instead of the one the program set. POSIX systems end every thread with the process and keep the code given: nothing
 * to wait for. Called when those threads are known to be leaving (the program's end waited for them). */
void xb_platform_threads_wait(void);
void xb_thread_yield(void);

/* The calling thread's stack as the system reserved it: `*low` the lowest address code may use (above its guard
 * pages), `*high` the end of the stack (it grows down from there): the stack error's limit (core/ctx.c) and the range
 * the collector scans (gc.c). 0, both NULL, when the system does not tell. */
int xb_platform_stack_bounds(const char **low, const char **high);

/* The calling thread's stack between `from` (its lowest address the program may use) and `to`, below every frame of
 * the thread (both whole pages), is used no more: its pages go back to the system and come back, zeroed or not, as the
 * stack grows into them again (on Windows, through a guard page set just below `to`, the thread's stack limit moved
 * up there, as a stack that never grew so deep has them). */
void xb_platform_stack_give_back(const char *from, const char *to);

/* The processors the program may run on (at least 1): those of its affinity where the system tells it (Linux), else
 * every processor online. */
unsigned xb_platform_cpu_count(void);

/* Memory for the collector's heap, in whole pages of the system, which it can give back. xb_platform_pages_map maps
 * `size` bytes (a multiple of the page size) readable, writable and zeroed, at an address aligned to `align` (a power
 * of two, a multiple of the page size): NULL when the system has no memory; xb_platform_pages_unmap gives a mapping
 * back whole (its address and size). xb_platform_pages_decommit lets the system take back the pages of [p, p + size),
 * which stay the caller's address range but lose their contents (the process's resident memory drops); before they are
 * used again, xb_platform_pages_recommit makes them readable and writable, zeroed or not (0: the system has no memory).
 * Decommit takes whole pages; recommit rounds its range out to the pages it touches. xb_platform_page_size: the
 * system's page size, a power of two. */
void *xb_platform_pages_map(size_t size, size_t align);
void xb_platform_pages_unmap(void *p, size_t size);
void xb_platform_pages_decommit(void *p, size_t size);
int xb_platform_pages_recommit(void *p, size_t size);
size_t xb_platform_page_size(void);

/* The memory of the process that is resident now (its working set), in bytes: 0 when the system does not tell. */
size_t xb_platform_resident_bytes(void);

/* The address space the system lets the process map, in bytes (a limit set on it, RLIMIT_AS; Windows' virtual space
 * of the process): SIZE_MAX when it tells none. */
size_t xb_platform_address_space(void);

/* One-time process setup, first thing in main. */
void xb_platform_init(void);

/* Nanoseconds of a monotonic clock (never set back, unaffected by changes of the date and time), from an origin of its
 * own: for intervals and deadlines. Usable before xb_platform_init too. */
int64_t xb_platform_monotonic_ns(void);

/* Bytes from the system's source of randomness (/dev/urandom; rand_s on Windows) into buf, at most n: how many it
 * gave (fewer when it cannot be read, as in a chroot without /dev). */
size_t xb_platform_entropy(void *buf, size_t n);
/* Fills buf with n random bytes (platform/random.c): the system's, the rest from the clock and addresses of the
 * process's memory, which no one outside the process can predict either. */
void xb_platform_random(void *buf, size_t n);

/* Calls fn(arg) with the thread's registers saved on its stack first, so a scan of the stack from inside fn sees every
 * pointer the thread holds (the collector's scan for pinned objects). The stack grows down on every target. */
void xb_platform_with_registers(void (*fn)(void *), void *arg);

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

/* The system's end of a line of text ("\n"; "\r\n" on Windows): the lines of the text files tables are copied to. */
const char *xb_platform_eol(void);

/* Files. A file is the system's handle as an integer; paths are UTF-8. A
 * function that fails returns 0 (a count or position: -1) and stores in
 * `*err` the code that FError() reports: a DOS error number, which Windows
 * reports itself and the POSIX errno values are translated to (2 file not
 * found, 3 path not found, 4 too many open files, 5 access denied, 6 invalid
 * handle, 8 not enough memory, 12 invalid access code, 15 invalid drive, 25
 * seek error, 32 sharing violation, 80 file exists, 87 invalid parameter, 112
 * disk full, 206 name too long; 31, general failure, for the others). */
typedef intptr_t xb_file;
#define XB_FERR_ACCESS 5
#define XB_FERR_FAILURE 31
#define XB_FERR_DISK_FULL 112

/* File attributes, as DOS numbers them: those FCreate() takes (fileio.ch),
 * and those Directory() tells besides. */
#define XB_FA_READONLY FC_READONLY
#define XB_FA_HIDDEN FC_HIDDEN
#define XB_FA_SYSTEM FC_SYSTEM
#define XB_FA_DIRECTORY 0x10
#define XB_FA_ARCHIVE 0x20

/* Opens an existing file (not a directory: 5) with an FOpen() mode
 * (fileio.ch: the access, 0 to 2, plus the sharing, which POSIX systems
 * ignore). */
int xb_platform_file_open(const char *path, int32_t mode, xb_file *f, int32_t *err);

/* Opens a file for reading and writing with an FOpen() sharing (as xb_platform_file_open), creating it, empty, when
 * there is none; a file that exists is not emptied. */
int xb_platform_file_open_always(const char *path, int32_t mode, xb_file *f, int32_t *err);

/* Creates a file, or empties one that exists, open for reading and writing;
 * `attr` may make it read-only, hidden or system (XB_FA_*; POSIX keeps the
 * read-only one only). */
int xb_platform_file_create(const char *path, int32_t attr, xb_file *f, int32_t *err);

/* Reads up to `n` bytes: the count read, 0 at the end of the file. */
int64_t xb_platform_file_read(xb_file f, void *buf, size_t n, int32_t *err);

/* Writes the `n` bytes: 1 when all of them were written. */
int xb_platform_file_write(xb_file f, const void *buf, size_t n, int32_t *err);

/* The same at `pos` from the start, in one call (the position after is not specified): the reads and writes of
 * tables, memos and indexes. */
int64_t xb_platform_file_read_at(xb_file f, int64_t pos, void *buf, size_t n, int32_t *err);
int xb_platform_file_write_at(xb_file f, int64_t pos, const void *buf, size_t n, int32_t *err);

/* Moves to `offset` from the start (0), the current position (1) or the end
 * (2): the new position; before the start is 25. */
int64_t xb_platform_file_seek(xb_file f, int64_t offset, int origin, int32_t *err);

/* Makes the file `size` bytes long (the position stays where it was). */
int xb_platform_file_truncate(xb_file f, int64_t size, int32_t *err);

/* Closes the file: 1 when nothing failed. */
int xb_platform_file_close(xb_file f, int32_t *err);

/* Has the system write the file's data that it holds to the disk (COMMIT): 1 when nothing failed. */
int xb_platform_file_sync(xb_file f, int32_t *err);

/* Sharing and locks of the files of tables, memos and indexes (the database layer's protocol, docs/rdd.md).
 *
 * xb_platform_file_share: the file's sharing, taken after opening: XB_LOCK_WRITE exclusive, XB_LOCK_READ shared,
 * XB_LOCK_NONE given up; not waiting; refused: 32. POSIX takes a whole-file lock of its own kind for it (`flock`,
 * which belongs to the handle); on Windows the sharing is the share mode the file was opened with (FO_EXCLUSIVE or
 * FO_DENYNONE) and this does nothing.
 *
 * xb_platform_file_lock: a byte-range lock of `len` bytes at `pos`: XB_LOCK_READ (others may read-lock too),
 * XB_LOCK_WRITE, or XB_LOCK_NONE to release; never waiting: another process's conflicting lock fails at once with 33.
 * On POSIX systems the locks belong to the process: taking a range it holds changes its kind, releasing part of one
 * releases that part, and closing any handle of the file releases them all. On Windows they belong to the handle, and a
 * release names exactly a range locked before (xb_platform_locks_per_handle). */
#define XB_LOCK_NONE 0
#define XB_LOCK_READ 1
#define XB_LOCK_WRITE 2
#define XB_FERR_LOCKED 33
int xb_platform_file_share(xb_file f, int kind, int32_t *err);
int xb_platform_file_lock(xb_file f, uint64_t pos, uint64_t len, int kind, int32_t *err);
/* Whether locks belong to a handle (Windows) rather than to the process (POSIX). */
int xb_platform_locks_per_handle(void);

/* What tells one file from another whatever the path that names it: the device and inode on POSIX systems, the
 * volume serial number and the file index on Windows. */
typedef struct xb_file_id {
  uint64_t volume, index;
} xb_file_id;
int xb_platform_file_id(xb_file f, xb_file_id *id, int32_t *err);
/* The same for the file a path names, without opening it for reading or writing. */
int xb_platform_path_id(const char *path, xb_file_id *id, int32_t *err);

/* Lets other threads and processes run for about `ms` milliseconds (0: gives up the rest of the time slice). */
void xb_platform_sleep_ms(uint32_t ms);

/* The system's own wait on an address (Linux's futex, Windows' WaitOnAddress), where it has one: XB_PLATFORM_ADDR_WAIT
 * is 1 there, and the parking lot (core/sync.c) sleeps on it; elsewhere on condition variables. xb_platform_addr_wait
 * sleeps while the 32-bit value at `addr` is `expected`, until xb_platform_addr_wake of the address wakes the threads
 * sleeping on it, or `ns` nanoseconds (ns < 0: no limit), or for no reason: the caller tests its condition again. */
#if defined(__linux__) || defined(_WIN32)
#define XB_PLATFORM_ADDR_WAIT 1
void xb_platform_addr_wait(const uint32_t *addr, uint32_t expected, int64_t ns);
void xb_platform_addr_wake(const uint32_t *addr);
#else
#define XB_PLATFORM_ADDR_WAIT 0
#endif

/* Deletes a file (not a directory: 5). */
int xb_platform_file_erase(const char *path, int32_t *err);

/* Renames a file or directory; a new name that is another file that exists
 * is 5 (nothing is replaced). */
int xb_platform_file_rename(const char *from, const char *to, int32_t *err);

/* What Directory() tells of a file: its name (UTF-8, without the directory),
 * size, attributes (XB_FA_*) and time of last change, in local time. POSIX
 * systems mark names that start with a dot hidden (but "." and ".."), files
 * the owner cannot write read-only, and regular files archive. */
typedef struct xb_dir_entry {
  const char *name;
  int64_t size;
  uint32_t attrs;
  int year, month, day, hour, minute, second;
} xb_dir_entry;

/* The entry of one path, whose last part, the entry's name, is `name`. */
int xb_platform_file_info(const char *path, const char *name, xb_dir_entry *e, int32_t *err);

/* The entries of a directory ("" is the current one), "." and ".." included,
 * one per xb_platform_dir_next (0 after the last); an entry's name lives
 * until the next call. */
typedef struct xb_dir_impl *xb_dir;
int xb_platform_dir_open(const char *path, xb_dir *d, int32_t *err);
int xb_platform_dir_next(xb_dir d, xb_dir_entry *e);
void xb_platform_dir_close(xb_dir d);

/* Whether the file system of a directory ("" is the current one) tells names
 * apart by letter case (Windows: no; macOS: as the volume says; other POSIX
 * systems: yes). */
int xb_platform_names_case_sensitive(const char *dir);

/* The current directory, malloc'ed (NULL on failure); changing it. */
char *xb_platform_cwd(int32_t *err);
int xb_platform_chdir(const char *path, int32_t *err);

/* The bytes free on drive `drive` (0: the current one; on Windows 1 is A:,
 * 2 is B: and so on; elsewhere, without drives, the file system of the
 * current directory for any of them). */
int xb_platform_disk_free(int drive, int64_t *bytes, int32_t *err);

/* The value of an environment variable, malloc'ed; NULL when it is not set.
 * For GetEnv(), a language feature: the runtime never reads one to configure
 * itself. */
char *xb_platform_getenv(const char *name);

/* Runs a command line through the system's shell (sh -c; cmd.exe on Windows), its output on the program's, and waits
 * for it to end: its exit status, -1 when it could not start. For RUN, a language feature. */
int xb_platform_run(const char *command);

#endif
