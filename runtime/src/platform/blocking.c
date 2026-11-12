/* The platform functions that may block the thread, the same on every platform: each one runs the operating
 * system's side (os.h) through xb_blocking_run, its arguments and result in a structure, so that a collection
 * never waits for it. */
#include "platform/os.h"

typedef struct os_file_open {
  const char *path;
  int32_t mode;
  xb_file *f;
  int32_t *err;
  int r;
} os_file_open;

static void run_file_open(void *p) {
  os_file_open *a = p;
  a->r = xb_os_file_open(a->path, a->mode, a->f, a->err);
}

int xb_platform_file_open(const char *path, int32_t mode, xb_file *f, int32_t *err) {
  os_file_open a = {path, mode, f, err, 0};
  xb_blocking_run(run_file_open, &a);
  return a.r;
}

typedef struct os_file_open_always {
  const char *path;
  int32_t mode;
  xb_file *f;
  int32_t *err;
  int r;
} os_file_open_always;

static void run_file_open_always(void *p) {
  os_file_open_always *a = p;
  a->r = xb_os_file_open_always(a->path, a->mode, a->f, a->err);
}

int xb_platform_file_open_always(const char *path, int32_t mode, xb_file *f, int32_t *err) {
  os_file_open_always a = {path, mode, f, err, 0};
  xb_blocking_run(run_file_open_always, &a);
  return a.r;
}

typedef struct os_file_create {
  const char *path;
  int32_t attr;
  xb_file *f;
  int32_t *err;
  int r;
} os_file_create;

static void run_file_create(void *p) {
  os_file_create *a = p;
  a->r = xb_os_file_create(a->path, a->attr, a->f, a->err);
}

int xb_platform_file_create(const char *path, int32_t attr, xb_file *f, int32_t *err) {
  os_file_create a = {path, attr, f, err, 0};
  xb_blocking_run(run_file_create, &a);
  return a.r;
}

typedef struct os_file_read {
  xb_file f;
  void *buf;
  size_t n;
  int32_t *err;
  int64_t r;
} os_file_read;

static void run_file_read(void *p) {
  os_file_read *a = p;
  a->r = xb_os_file_read(a->f, a->buf, a->n, a->err);
}

int64_t xb_platform_file_read(xb_file f, void *buf, size_t n, int32_t *err) {
  os_file_read a = {f, buf, n, err, 0};
  xb_blocking_run(run_file_read, &a);
  return a.r;
}

typedef struct os_file_write {
  xb_file f;
  const void *buf;
  size_t n;
  int32_t *err;
  int r;
} os_file_write;

static void run_file_write(void *p) {
  os_file_write *a = p;
  a->r = xb_os_file_write(a->f, a->buf, a->n, a->err);
}

int xb_platform_file_write(xb_file f, const void *buf, size_t n, int32_t *err) {
  os_file_write a = {f, buf, n, err, 0};
  xb_blocking_run(run_file_write, &a);
  return a.r;
}

typedef struct os_file_read_at {
  xb_file f;
  int64_t pos;
  void *buf;
  size_t n;
  int32_t *err;
  int64_t r;
} os_file_read_at;

static void run_file_read_at(void *p) {
  os_file_read_at *a = p;
  a->r = xb_os_file_read_at(a->f, a->pos, a->buf, a->n, a->err);
}

int64_t xb_platform_file_read_at(xb_file f, int64_t pos, void *buf, size_t n, int32_t *err) {
  os_file_read_at a = {f, pos, buf, n, err, 0};
  xb_blocking_run(run_file_read_at, &a);
  return a.r;
}

typedef struct os_file_write_at {
  xb_file f;
  int64_t pos;
  const void *buf;
  size_t n;
  int32_t *err;
  int r;
} os_file_write_at;

static void run_file_write_at(void *p) {
  os_file_write_at *a = p;
  a->r = xb_os_file_write_at(a->f, a->pos, a->buf, a->n, a->err);
}

int xb_platform_file_write_at(xb_file f, int64_t pos, const void *buf, size_t n, int32_t *err) {
  os_file_write_at a = {f, pos, buf, n, err, 0};
  xb_blocking_run(run_file_write_at, &a);
  return a.r;
}

typedef struct os_file_seek {
  xb_file f;
  int64_t offset;
  int origin;
  int32_t *err;
  int64_t r;
} os_file_seek;

static void run_file_seek(void *p) {
  os_file_seek *a = p;
  a->r = xb_os_file_seek(a->f, a->offset, a->origin, a->err);
}

int64_t xb_platform_file_seek(xb_file f, int64_t offset, int origin, int32_t *err) {
  os_file_seek a = {f, offset, origin, err, 0};
  xb_blocking_run(run_file_seek, &a);
  return a.r;
}

typedef struct os_file_truncate {
  xb_file f;
  int64_t size;
  int32_t *err;
  int r;
} os_file_truncate;

static void run_file_truncate(void *p) {
  os_file_truncate *a = p;
  a->r = xb_os_file_truncate(a->f, a->size, a->err);
}

int xb_platform_file_truncate(xb_file f, int64_t size, int32_t *err) {
  os_file_truncate a = {f, size, err, 0};
  xb_blocking_run(run_file_truncate, &a);
  return a.r;
}

typedef struct os_file_close {
  xb_file f;
  int32_t *err;
  int r;
} os_file_close;

static void run_file_close(void *p) {
  os_file_close *a = p;
  a->r = xb_os_file_close(a->f, a->err);
}

int xb_platform_file_close(xb_file f, int32_t *err) {
  os_file_close a = {f, err, 0};
  xb_blocking_run(run_file_close, &a);
  return a.r;
}

typedef struct os_file_share {
  xb_file f;
  int kind;
  int32_t *err;
  int r;
} os_file_share;

static void run_file_share(void *p) {
  os_file_share *a = p;
  a->r = xb_os_file_share(a->f, a->kind, a->err);
}

int xb_platform_file_share(xb_file f, int kind, int32_t *err) {
  os_file_share a = {f, kind, err, 0};
  xb_blocking_run(run_file_share, &a);
  return a.r;
}

typedef struct os_file_lock {
  xb_file f;
  uint64_t pos;
  uint64_t len;
  int kind;
  int32_t *err;
  int r;
} os_file_lock;

static void run_file_lock(void *p) {
  os_file_lock *a = p;
  a->r = xb_os_file_lock(a->f, a->pos, a->len, a->kind, a->err);
}

int xb_platform_file_lock(xb_file f, uint64_t pos, uint64_t len, int kind, int32_t *err) {
  os_file_lock a = {f, pos, len, kind, err, 0};
  xb_blocking_run(run_file_lock, &a);
  return a.r;
}

typedef struct os_file_id {
  xb_file f;
  xb_file_id *id;
  int32_t *err;
  int r;
} os_file_id;

static void run_file_id(void *p) {
  os_file_id *a = p;
  a->r = xb_os_file_id(a->f, a->id, a->err);
}

int xb_platform_file_id(xb_file f, xb_file_id *id, int32_t *err) {
  os_file_id a = {f, id, err, 0};
  xb_blocking_run(run_file_id, &a);
  return a.r;
}

typedef struct os_path_id {
  const char *path;
  xb_file_id *id;
  int32_t *err;
  int r;
} os_path_id;

static void run_path_id(void *p) {
  os_path_id *a = p;
  a->r = xb_os_path_id(a->path, a->id, a->err);
}

int xb_platform_path_id(const char *path, xb_file_id *id, int32_t *err) {
  os_path_id a = {path, id, err, 0};
  xb_blocking_run(run_path_id, &a);
  return a.r;
}

typedef struct os_file_erase {
  const char *path;
  int32_t *err;
  int r;
} os_file_erase;

static void run_file_erase(void *p) {
  os_file_erase *a = p;
  a->r = xb_os_file_erase(a->path, a->err);
}

int xb_platform_file_erase(const char *path, int32_t *err) {
  os_file_erase a = {path, err, 0};
  xb_blocking_run(run_file_erase, &a);
  return a.r;
}

typedef struct os_file_rename {
  const char *from;
  const char *to;
  int32_t *err;
  int r;
} os_file_rename;

static void run_file_rename(void *p) {
  os_file_rename *a = p;
  a->r = xb_os_file_rename(a->from, a->to, a->err);
}

int xb_platform_file_rename(const char *from, const char *to, int32_t *err) {
  os_file_rename a = {from, to, err, 0};
  xb_blocking_run(run_file_rename, &a);
  return a.r;
}

typedef struct os_file_info {
  const char *path;
  const char *name;
  xb_dir_entry *e;
  int32_t *err;
  int r;
} os_file_info;

static void run_file_info(void *p) {
  os_file_info *a = p;
  a->r = xb_os_file_info(a->path, a->name, a->e, a->err);
}

int xb_platform_file_info(const char *path, const char *name, xb_dir_entry *e, int32_t *err) {
  os_file_info a = {path, name, e, err, 0};
  xb_blocking_run(run_file_info, &a);
  return a.r;
}

typedef struct os_dir_open {
  const char *path;
  xb_dir *d;
  int32_t *err;
  int r;
} os_dir_open;

static void run_dir_open(void *p) {
  os_dir_open *a = p;
  a->r = xb_os_dir_open(a->path, a->d, a->err);
}

int xb_platform_dir_open(const char *path, xb_dir *d, int32_t *err) {
  os_dir_open a = {path, d, err, 0};
  xb_blocking_run(run_dir_open, &a);
  return a.r;
}

typedef struct os_dir_next {
  xb_dir d;
  xb_dir_entry *e;
  int r;
} os_dir_next;

static void run_dir_next(void *p) {
  os_dir_next *a = p;
  a->r = xb_os_dir_next(a->d, a->e);
}

int xb_platform_dir_next(xb_dir d, xb_dir_entry *e) {
  os_dir_next a = {d, e, 0};
  xb_blocking_run(run_dir_next, &a);
  return a.r;
}

static void run_dir_close(void *d) { xb_os_dir_close(d); }

void xb_platform_dir_close(xb_dir d) { xb_blocking_run(run_dir_close, d); }

typedef struct os_names_case_sensitive {
  const char *dir;
  int r;
} os_names_case_sensitive;

static void run_names_case_sensitive(void *p) {
  os_names_case_sensitive *a = p;
  a->r = xb_os_names_case_sensitive(a->dir);
}

int xb_platform_names_case_sensitive(const char *dir) {
  os_names_case_sensitive a = {dir, 0};
  xb_blocking_run(run_names_case_sensitive, &a);
  return a.r;
}

typedef struct os_cwd {
  int32_t *err;
  char *r;
} os_cwd;

static void run_cwd(void *p) {
  os_cwd *a = p;
  a->r = xb_os_cwd(a->err);
}

char *xb_platform_cwd(int32_t *err) {
  os_cwd a = {err, NULL};
  xb_blocking_run(run_cwd, &a);
  return a.r;
}

typedef struct os_chdir {
  const char *path;
  int32_t *err;
  int r;
} os_chdir;

static void run_chdir(void *p) {
  os_chdir *a = p;
  a->r = xb_os_chdir(a->path, a->err);
}

int xb_platform_chdir(const char *path, int32_t *err) {
  os_chdir a = {path, err, 0};
  xb_blocking_run(run_chdir, &a);
  return a.r;
}

typedef struct os_disk_free {
  int drive;
  int64_t *bytes;
  int32_t *err;
  int r;
} os_disk_free;

static void run_disk_free(void *p) {
  os_disk_free *a = p;
  a->r = xb_os_disk_free(a->drive, a->bytes, a->err);
}

int xb_platform_disk_free(int drive, int64_t *bytes, int32_t *err) {
  os_disk_free a = {drive, bytes, err, 0};
  xb_blocking_run(run_disk_free, &a);
  return a.r;
}

static void run_flush(void *stream) { fflush(stream); }

void xb_platform_flush(FILE *stream) { xb_blocking_run(run_flush, stream); }
