/* The registry of shared files and their locks (core/shared_file.h).
 *
 * On POSIX systems record locks belong to the process: the OS never sets two users of the process against each
 * other, and closing any handle of a file drops all the process's locks on it. So the users' locks are kept here and
 * checked against each other first; the OS is asked only for bytes the process does not hold yet, and given back only
 * bytes no user holds any more. A handle is never closed while the file has users: one a user leaves is kept for the
 * next user of the same access, and an opening is refused (from the file's identity, found by its path) before any
 * handle is opened. On Windows locks belong to a handle: each user has its own, and its locks are taken and released
 * as asked.
 *
 * Waiting locks are retried with a growing pause, the registry's mutex released meanwhile. */
#include "core/shared_file.h"
#include "core/sync.h"

#include "gc/gc.h"

#include <stdlib.h>
#include <string.h>

typedef struct lock_rec {
  uint64_t pos, end; /* the bytes pos .. end - 1 */
  int kind;
  const xb_shared *owner;
} lock_rec;

typedef struct entry {
  xb_file_id id;
  struct entry *next;
  uint32_t users;
  int exclusive;    /* its one user has it exclusive */
  lock_rec *locks;
  uint32_t nlocks, lockCap;
  struct parked {   /* POSIX: handles of users that left, for the next users, closed with the entry */
    xb_file f;
    int readonly;
  } *parked;
  uint32_t nparked, parkedCap;
} entry;

struct xb_shared {
  entry *e;
  xb_file f;
  int exclusive, readonly;
};

static struct {
  xb_mutex lock;
  entry *files;
} registry;

static void init_registry(void) {
  if (!xb_mutex_init(&registry.lock)) xb_gc_fatal("cannot create the file registry lock");
}

void xb_shared_init(void) {
  static xb_once_flag made = XB_ONCE_INIT;
  xb_once(&made, init_registry);
}

static void *grow(void *p, uint32_t *cap, size_t size) {
  uint32_t n = *cap ? *cap * 2 : 8;
  void *q = realloc(p, n * size);
  if (!q) xb_gc_fatal("out of memory");
  *cap = n;
  return q;
}

static entry *find(const xb_file_id *id) {
  for (entry *e = registry.files; e; e = e->next)
    if (e->id.volume == id->volume && e->id.index == id->index) return e;
  return NULL;
}

static void entry_free(entry *e) {
  entry **p = &registry.files;
  while (*p != e) p = &(*p)->next;
  *p = e->next;
  for (uint32_t i = 0; i < e->nparked; ++i) {
    int32_t ignored;
    xb_platform_file_close(e->parked[i].f, &ignored);
  }
  free(e->parked);
  free(e->locks);
  free(e);
}

static void park(entry *e, xb_file f, int readonly) {
  if (e->nparked == e->parkedCap) e->parked = grow(e->parked, &e->parkedCap, sizeof *e->parked);
  e->parked[e->nparked].f = f;
  e->parked[e->nparked++].readonly = readonly;
}

/* A kept handle of the entry for the access asked; 0 for none. */
static int unpark(entry *e, int readonly, xb_file *f) {
  for (uint32_t i = 0; i < e->nparked; ++i)
    if (e->parked[i].readonly == readonly) {
      *f = e->parked[i].f;
      e->parked[i] = e->parked[--e->nparked];
      return 1;
    }
  return 0;
}

/* A handle opened for a user that will not have it: kept when the file has users (POSIX: closing it would drop their
 * locks), else closed. */
static void give_up(entry *e, xb_file f, int readonly) {
  if (e && !xb_platform_locks_per_handle()) {
    park(e, f, readonly);
    return;
  }
  int32_t ignored;
  xb_platform_file_close(f, &ignored);
}

static int sharing_refused(const entry *e, int exclusive, int32_t *err) {
  if (!e || (!e->exclusive && !exclusive)) return 0; /* inside the process, as the OS gives it outside */
  *err = 32;
  return 1;
}

int xb_shared_open(const char *path, int readonly, int exclusive, int create, xb_shared **out, int32_t *err) {
  if (create) {
    readonly = 0;
    exclusive = 1;
  }
  int32_t mode = (readonly ? FO_READ : FO_READWRITE) | (exclusive ? FO_EXCLUSIVE : FO_DENYNONE);
  xb_file_id id;
  int32_t idErr = 0;
  int known = xb_platform_path_id(path, &id, &idErr);
  if (!known && !(create && idErr == 2)) { /* a file that is not there (but to create), or not reachable */
    *err = idErr;
    return 0;
  }
  xb_mutex_lock(&registry.lock);
  entry *e = known ? find(&id) : NULL;
  xb_file f;
  int ok = !sharing_refused(e, exclusive, err);
  int reused = ok && e && !xb_platform_locks_per_handle() && unpark(e, readonly, &f);
  if (ok && !reused)
    ok = create ? xb_platform_file_open_always(path, mode, &f, err) : xb_platform_file_open(path, mode, &f, err);
  int opened = ok;
  if (ok && !known) { /* created now */
    ok = xb_platform_file_id(f, &id, err);
    e = ok ? find(&id) : NULL;
    ok = ok && !sharing_refused(e, exclusive, err);
  }
  ok = ok && xb_platform_file_share(f, exclusive ? XB_LOCK_WRITE : XB_LOCK_READ, err);
  if (ok && create) ok = xb_platform_file_truncate(f, 0, err);
  if (!ok) {
    if (opened) give_up(e, f, readonly);
    xb_mutex_unlock(&registry.lock);
    return 0;
  }
  if (!e) {
    e = calloc(1, sizeof *e);
    if (!e) xb_gc_fatal("out of memory");
    e->id = id;
    e->next = registry.files;
    registry.files = e;
  }
  xb_shared *s = malloc(sizeof *s);
  if (!s) xb_gc_fatal("out of memory");
  s->e = e;
  s->f = f;
  s->exclusive = exclusive;
  s->readonly = readonly;
  ++e->users;
  e->exclusive = exclusive;
  xb_mutex_unlock(&registry.lock);
  *out = s;
  return 1;
}

xb_file xb_shared_handle(const xb_shared *s) { return s->f; }

int xb_shared_exclusive(const xb_shared *s) { return s->exclusive; }

/* The strongest kind the process holds on byte `at` (XB_LOCK_NONE for none), leaving out `skip`. */
static int held_at(const entry *e, uint64_t at, const lock_rec *skip) {
  int kind = XB_LOCK_NONE;
  for (uint32_t i = 0; i < e->nlocks; ++i) {
    const lock_rec *r = &e->locks[i];
    if (r != skip && r->pos <= at && at < r->end && r->kind > kind) kind = r->kind;
  }
  return kind;
}

/* The next place after `at` (and before `end`) where some lock starts or ends: what the process holds is the same
 * from `at` up to there. */
static uint64_t next_edge(const entry *e, uint64_t at, uint64_t end) {
  uint64_t next = end;
  for (uint32_t i = 0; i < e->nlocks; ++i) {
    const lock_rec *r = &e->locks[i];
    if (r->pos > at && r->pos < next) next = r->pos;
    if (r->end > at && r->end < next) next = r->end;
  }
  return next;
}

/* POSIX: sets what the OS holds for the process on pos .. end - 1 to what the users' locks (but `skip`) need, where
 * it is less than `kind` (taking) or where `skip` gave more (giving back, `kind` XB_LOCK_NONE). 0 when the OS refused
 * a part: what was taken is given back. */
static int os_take(entry *e, xb_file f, uint64_t pos, uint64_t end, int kind, int32_t *err) {
  for (uint64_t at = pos; at < end;) {
    uint64_t next = next_edge(e, at, end);
    if (held_at(e, at, NULL) < kind && !xb_platform_file_lock(f, at, next - at, kind, err)) {
      for (uint64_t back = pos; back < at;) { /* what this call took goes back to what the others hold */
        uint64_t stop = next_edge(e, back, at);
        int before = held_at(e, back, NULL);
        int32_t ignored;
        if (before < kind) xb_platform_file_lock(f, back, stop - back, before, &ignored);
        back = stop;
      }
      return 0;
    }
    at = next;
  }
  return 1;
}

static void os_give_back(entry *e, xb_file f, const lock_rec *gone) {
  for (uint64_t at = gone->pos; at < gone->end;) {
    uint64_t next = next_edge(e, at, gone->end);
    int still = held_at(e, at, gone);
    int32_t ignored;
    if (still < gone->kind) xb_platform_file_lock(f, at, next - at, still, &ignored);
    at = next;
  }
}

/* One try: 1 had, 0 held by another user of the process, -1 refused by the OS (33) or failed. */
static int try_lock(xb_shared *s, uint64_t pos, uint64_t end, int kind, int32_t *err) {
  entry *e = s->e;
  for (uint32_t i = 0; i < e->nlocks; ++i) {
    const lock_rec *r = &e->locks[i];
    if (r->pos >= end || r->end <= pos) continue;
    if (r->owner == s && r->pos == pos && r->end == end && r->kind >= kind) return 1; /* held already */
    if (r->owner != s && (r->kind == XB_LOCK_WRITE || kind == XB_LOCK_WRITE)) {
      *err = XB_FERR_LOCKED;
      return 0;
    }
  }
  int ok = xb_platform_locks_per_handle() ? xb_platform_file_lock(s->f, pos, end - pos, kind, err)
                                          : os_take(e, s->f, pos, end, kind, err);
  if (!ok) return -1;
  if (e->nlocks == e->lockCap) e->locks = grow(e->locks, &e->lockCap, sizeof *e->locks);
  e->locks[e->nlocks++] = (lock_rec){pos, end, kind, s};
  return 1;
}

int xb_shared_lock(xb_shared *s, uint64_t pos, uint64_t len, int kind, int wait, int32_t *err) {
  if (s->exclusive) return 1;
  for (uint32_t pause = 0;; pause = pause < 32 ? pause * 2 + 1 : 63) {
    xb_mutex_lock(&registry.lock);
    int r = try_lock(s, pos, pos + len, kind, err);
    xb_mutex_unlock(&registry.lock);
    if (r >= 0 || !wait || *err != XB_FERR_LOCKED) return r > 0;
    xb_platform_sleep_ms(pause); /* another program holds it: again, after a pause of up to 63 ms */
  }
}

/* Removes lock `i` of the entry, giving back to the OS what nobody else holds. */
static int drop(entry *e, xb_shared *s, uint32_t i, int32_t *err) {
  lock_rec gone = e->locks[i];
  int ok = 1;
  if (xb_platform_locks_per_handle())
    ok = xb_platform_file_lock(s->f, gone.pos, gone.end - gone.pos, XB_LOCK_NONE, err);
  else
    os_give_back(e, s->f, &e->locks[i]);
  e->locks[i] = e->locks[--e->nlocks];
  return ok;
}

int xb_shared_unlock(xb_shared *s, uint64_t pos, uint64_t len, int32_t *err) {
  if (s->exclusive) return 1;
  entry *e = s->e;
  xb_mutex_lock(&registry.lock);
  int ok = 1;
  for (uint32_t i = 0; i < e->nlocks; ++i) {
    const lock_rec *r = &e->locks[i];
    if (r->owner == s && r->pos == pos && r->end == pos + len) {
      ok = drop(e, s, i, err);
      break;
    }
  }
  xb_mutex_unlock(&registry.lock);
  return ok;
}

int xb_shared_close(xb_shared *s, int32_t *err) {
  entry *e = s->e;
  xb_mutex_lock(&registry.lock);
  int ok = 1;
  for (uint32_t i = 0; i < e->nlocks;) {
    if (e->locks[i].owner == s) ok = drop(e, s, i, err) && ok;
    else ++i;
  }
  if (--e->users == 0 || xb_platform_locks_per_handle()) {
    ok = xb_platform_file_close(s->f, err) && ok;
  } else { /* POSIX: closing it would drop the other users' locks; its sharing goes now, the handle is kept */
    int32_t ignored;
    xb_platform_file_share(s->f, XB_LOCK_NONE, &ignored);
    park(e, s->f, s->readonly);
  }
  if (e->users == 0) entry_free(e);
  xb_mutex_unlock(&registry.lock);
  free(s);
  return ok;
}
