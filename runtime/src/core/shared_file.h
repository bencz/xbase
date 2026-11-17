/* The files of tables, memos and indexes, opened through the process's registry (core/shared_file.c; docs/rdd.md,
 * "Inside the process"): every opening of a file is a user of one entry, keyed by the file's identity, so that the
 * areas, tasks and threads of the program share a file and lock each other out as programs do. */
#ifndef XB_SHARED_FILE_H
#define XB_SHARED_FILE_H

#include "platform/platform.h"

typedef struct xb_shared xb_shared;

/* Sets up the registry: once, before other threads exist. */
void xb_shared_init(void);

/* Opens `path` for a new user, `readonly` or for reading and writing, `exclusive` or shared: a sharing that the
 * file's other users (in the process or not) do not allow is 32. With `create` the file is made, or emptied when it
 * exists and nobody uses it, opened exclusive. 0 after a failure (in `*err`). */
int xb_shared_open(const char *path, int readonly, int exclusive, int create, xb_shared **out, int32_t *err);
/* Gives up the user's locks and sharing, and closes its handle (on POSIX systems, when the file's last user goes:
 * closing a handle drops the process's locks). 1 when nothing failed. */
int xb_shared_close(xb_shared *s, int32_t *err);

/* The handle the user reads and writes with. */
xb_file xb_shared_handle(const xb_shared *s);
/* Whether the user has the file to itself (opened exclusive): its locks cost nothing then. */
int xb_shared_exclusive(const xb_shared *s);

/* Locks `len` bytes at `pos` for the user, XB_LOCK_READ or XB_LOCK_WRITE: 1, or 0 with 33 when another user (in the
 * process or not) holds a conflicting lock. `wait` waits for another process's lock (tried again after growing
 * pauses); one held inside the process is never waited for (with one thread it can only be the caller's own). Taking
 * again a range the user holds is 1. */
int xb_shared_lock(xb_shared *s, uint64_t pos, uint64_t len, int kind, int wait, int32_t *err);
/* Releases the user's lock of exactly that range (none: nothing). 1 when nothing failed. */
int xb_shared_unlock(xb_shared *s, uint64_t pos, uint64_t len, int32_t *err);

/* A file opened outside the registry: by the program (FOpen, FCreate) or by the runtime for a file of its own (a .mem
 * file, a text file). Each goes as xb_platform_file_open, _create and _close do, with the users of the registry in
 * mind. An opening that would keep the users from using the file is refused with 32, as another program would be
 * (FO_EXCLUSIVE or FO_DENYREAD; FO_DENYWRITE while a user writes), and so is any creation, which would empty their
 * file. On POSIX systems, where closing any handle of a file drops all the process's locks on it, a
 * handle closed while the file has users is kept with them (closed with the last one), so FClose() of a table's file
 * cannot drop the locks of an area that has it open; a later opening of the file for the same access (FO_READ,
 * FO_WRITE, FO_READWRITE) takes it again, from the file's start, so a program that opens and closes the file again
 * and again keeps one. Closing such a kept handle again, or a user's handle, is error 6, and xb_shared_plain_live
 * gives the handle the program's file functions use: `f`, or one that is not open (-1) for a kept handle, so that it
 * reads and writes nothing more once closed. */
int xb_shared_plain_open(const char *path, int32_t mode, xb_file *f, int32_t *err);
int xb_shared_plain_create(const char *path, int32_t attr, xb_file *f, int32_t *err);
int xb_shared_plain_close(xb_file f, int32_t *err);
xb_file xb_shared_plain_live(xb_file f);

#endif
