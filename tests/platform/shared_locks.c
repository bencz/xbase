/* Locks of the registry of shared files taken by threads at once, never waiting:
 * - on file A, four threads, each a user of its own, lock records (one byte each) of a few in common, for writing or
 *   for reading, and now and then a run of them (a range, for writing or reading, the whole run or part of it);
 * - on file A too, two threads open it again and again, lock records of their own and close it with the locks held
 *   (closing drops them) while the others lock;
 * - on file B, two threads lock as the four of A do: a file's locks never meet another's;
 * - on file C, two threads open, lock and close it again and again with nobody else, so that the file's last user
 *   leaves (its entry goes) while the other opens it or locks in it.
 * Each holder marks what it holds while it holds it (a writer alone, readers counted): a byte is never written by two
 * users nor read while written, the records a range covers never held against it, and nothing is left held at the
 * end: every byte of A, B and C locks at once for writing. */
#include "check.h"
#include "core/shared_file.h"
#include "xbrt_int.h"

#include <stdio.h>

#define ROUNDS 20000u
#define RECORDS 16u
#define BASE 1000u
#define WRITER 0x80000000u /* a mark's: held for writing; below it, the readers holding it */
#define FILES 3u

static const char *const files[FILES] = {"shared_locks_a.tmp", "shared_locks_b.tmp", "shared_locks_c.tmp"};
static uint32_t marks[FILES][RECORDS];
static uint32_t bad, failed, opened;

typedef struct job {
  uint32_t id;   /* from 1 */
  uint32_t file; /* an index of files */
} job;

static void wrong(void) { xb_atomic_add32(&bad, 1, XB_RELAXED); }

/* Marks records first .. first + n - 1 as held (for writing or reading): each one must allow it. */
static void mark(uint32_t *m, uint32_t first, uint32_t n, int write) {
  for (uint32_t r = first; r < first + n; ++r) {
    if (write) {
      uint32_t none = 0;
      if (!xb_atomic_cas32(&m[r], &none, WRITER, XB_ACQ_REL)) wrong();
    } else if (xb_atomic_add32(&m[r], 1, XB_ACQ_REL) & WRITER) {
      wrong();
    }
  }
}

static void unmark(uint32_t *m, uint32_t first, uint32_t n, int write) {
  for (uint32_t r = first; r < first + n; ++r)
    if (write) xb_atomic_store32(&m[r], 0, XB_RELEASE);
    else xb_atomic_add32(&m[r], (uint32_t)-1, XB_ACQ_REL);
}

/* Locks records first .. first + n - 1 (one lock), marks them, and lets them go. */
static void hold(xb_shared *s, uint32_t *m, uint32_t first, uint32_t n, int write) {
  int32_t err;
  if (!xb_shared_lock(s, BASE + first, n, write ? XB_LOCK_WRITE : XB_LOCK_READ, 0, &err)) {
    if (err != XB_FERR_LOCKED) wrong();
    return;
  }
  mark(m, first, n, write);
  unmark(m, first, n, write);
  if (!xb_shared_unlock(s, BASE + first, n, &err)) wrong();
}

static void locker(void *arg) {
  const job *j = arg;
  xb_gc_thread_attach(&arg);
  xb_ctx *ctx = xb_ctx_new();
  xb_shared *s;
  int32_t err;
  if (xb_shared_open(files[j->file], 0, 0, 0, &s, &err)) {
    uint32_t *m = marks[j->file];
    for (uint32_t round = 0; round < ROUNDS; ++round) {
      uint32_t r = (round * 7u + j->id * 3u) % RECORDS;
      int write = (round + j->id) % 3 != 0;
      if (round % 64 == 0) hold(s, m, 0, RECORDS, write); /* the whole run */
      else if (round % 16 == 0) hold(s, m, r / 2, RECORDS / 2, write); /* part of it */
      else hold(s, m, r, 1, write);
    }
    xb_shared_close(s, &err);
  } else {
    xb_atomic_add32(&failed, 1, XB_RELAXED);
  }
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

/* Opens the file, locks records of its own (a byte past the others') and closes it with them held. */
static void churner(void *arg) {
  const job *j = arg;
  xb_gc_thread_attach(&arg);
  xb_ctx *ctx = xb_ctx_new();
  for (uint32_t round = 0; round < ROUNDS / 20; ++round) {
    xb_shared *s;
    int32_t err;
    if (!xb_shared_open(files[j->file], 0, 0, 0, &s, &err)) {
      xb_atomic_add32(&failed, 1, XB_RELAXED);
      continue;
    }
    xb_atomic_add32(&opened, 1, XB_RELAXED);
    for (uint32_t k = 0; k < 8; ++k) {
      uint64_t pos = 2 * BASE + 100 * j->id + k;
      if (!xb_shared_lock(s, pos, 1, k % 2 ? XB_LOCK_READ : XB_LOCK_WRITE, 0, &err)) wrong();
    }
    if (!xb_shared_lock(s, 2 * BASE + 100 * j->id, 8, XB_LOCK_WRITE, 0, &err)) wrong(); /* over its own: allowed */
    xb_shared_close(s, &err);
  }
  xb_ctx_free(ctx);
  xb_gc_thread_detach();
}

int main(void) {
  xb_platform_init();
  xb_shared_init();
  int32_t err;
  for (uint32_t k = 0; k < FILES; ++k) {
    xb_file f;
    CHECK(xb_platform_file_create(files[k], 0, &f, &err) && xb_platform_file_close(f, &err));
  }
  enum { THREADS = 10 };
  job jobs[THREADS] = {{1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 1}, {8, 1}, {9, 2}, {10, 2}};
  void (*body[THREADS])(void *) = {locker, locker, locker, locker, churner, churner, locker, locker, churner, churner};
  xb_thread threads[THREADS];
  for (size_t k = 0; k < THREADS; ++k) CHECK(xb_thread_start(&threads[k], 0, body[k], &jobs[k]));
  for (size_t k = 0; k < THREADS; ++k) xb_thread_join(threads[k]);
  int base = 0;
  xb_gc_thread_attach(&base);
  CHECK(bad == 0);
  CHECK(failed == 0);
  CHECK(opened == 4 * (ROUNDS / 20));
  for (uint32_t k = 0; k < FILES; ++k) {
    xb_shared *s;
    CHECK(xb_shared_open(files[k], 0, 0, 0, &s, &err));
    CHECK(xb_shared_lock(s, 0, 4 * BASE, XB_LOCK_WRITE, 0, &err) && xb_shared_close(s, &err));
    xb_platform_file_erase(files[k], &err);
  }
  puts("shared_locks ok");
  return 0;
}
