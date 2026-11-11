/* Operating-system services. Each platform directory implements this
 * interface completely (posix/, win32/); nothing outside platform/ uses OS
 * APIs directly. */
#ifndef XB_PLATFORM_H
#define XB_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#include "fileio.ch"

/* XB_NOINLINE keeps a rare path out of a hot function, which then needs no
 * frame of its own. */
#if defined(_MSC_VER)
#define XB_THREAD_LOCAL __declspec(thread)
#define XB_NOINLINE __declspec(noinline)
#else
#define XB_THREAD_LOCAL _Thread_local
#define XB_NOINLINE __attribute__((noinline))
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

/* Files. A file is the system's handle as an integer; paths are UTF-8. A
 * function that fails returns 0 (a count or position: -1) and stores in
 * `*err` the code that FError() reports: a DOS error number, which Windows
 * reports itself and the POSIX errno values are translated to (2 file not
 * found, 3 path not found, 4 too many open files, 5 access denied, 6 invalid
 * handle, 8 not enough memory, 12 invalid access code, 15 invalid drive, 25
 * seek error, 32 sharing violation, 80 file exists, 87 invalid parameter, 112
 * disk full, 206 name too long; 31, general failure, for the others). */
typedef intptr_t xb_file;
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

#endif
