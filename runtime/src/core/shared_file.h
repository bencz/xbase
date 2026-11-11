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

#endif
