/* The POSIX interfaces this file uses (clock_gettime, localtime_r, threads,
 * files, directories), which a strict C11 build declares only on request;
 * 64-bit file offsets on 32-bit systems too; on macOS, the case sensitivity
 * of a volume (pathconf). */
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif

#include "platform/platform.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

static struct timespec started;

void xb_platform_init(void) { clock_gettime(CLOCK_MONOTONIC, &started); }

int64_t xb_platform_elapsed_ms(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  int64_t ns = (int64_t)(now.tv_sec - started.tv_sec) * 1000000000 + (now.tv_nsec - started.tv_nsec);
  return ns / 1000000; /* the total is never negative: whole milliseconds, rounded down */
}

typedef struct {
  void (*fn)(void *);
  void *arg;
} thread_start;

static void *run_thread(void *p) {
  thread_start *start = p;
  start->fn(start->arg);
  return NULL;
}

int xb_platform_run_on_stack(size_t stack_size, void (*fn)(void *), void *arg) {
  pthread_attr_t attr;
  pthread_t thread;
  thread_start start = {fn, arg};
  if (pthread_attr_init(&attr) != 0) return 0;
  int ok = pthread_attr_setstacksize(&attr, stack_size) == 0 && pthread_create(&thread, &attr, run_thread, &start) == 0;
  pthread_attr_destroy(&attr);
  if (ok) pthread_join(thread, NULL);
  return ok;
}

struct xb_mutex_impl {
  pthread_mutex_t m;
};

int xb_mutex_init(xb_mutex *m) {
  *m = malloc(sizeof **m);
  return *m && pthread_mutex_init(&(*m)->m, NULL) == 0;
}

void xb_mutex_lock(xb_mutex *m) { pthread_mutex_lock(&(*m)->m); }
void xb_mutex_unlock(xb_mutex *m) { pthread_mutex_unlock(&(*m)->m); }

void xb_platform_local_time(int *hour, int *minute, int *second, int *msec) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm;
  localtime_r(&ts.tv_sec, &tm);
  *hour = tm.tm_hour;
  *minute = tm.tm_min;
  *second = tm.tm_sec;
  *msec = (int)(ts.tv_nsec / 1000000);
}

void xb_platform_local_date(int *year, int *month, int *day) {
  time_t now = time(NULL);
  struct tm tm;
  localtime_r(&now, &tm);
  *year = tm.tm_year + 1900;
  *month = tm.tm_mon + 1;
  *day = tm.tm_mday;
}

/* The arguments are passed through as bytes: the program receives what the
 * shell gave it (UTF-8 under a UTF-8 locale). */
char **xb_platform_args(int argc, char **argv, int *count) {
  *count = argc;
  return argv;
}

int xb_platform_path_separator(char c) { return c == '/'; }

/* The DOS error number of an errno value (see platform.h). */
static int32_t dos_error(int e) {
  static const struct {
    int errno_value;
    int32_t dos;
  } table[] = {
    {ENOENT, 2},  {ENOTDIR, 3},  {EMFILE, 4},  {ENFILE, 4},   {EACCES, 5}, {EPERM, 5},
    {EROFS, 5},   {EISDIR, 5},   {EBADF, 6},   {ENOMEM, 8},   {EBUSY, 32}, {ETXTBSY, 32},
    {EEXIST, 80}, {EINVAL, 87},  {ENOSPC, 112}, {ENAMETOOLONG, 206},
#ifdef EDQUOT /* macOS declares it only outside strict POSIX */
    {EDQUOT, 112},
#endif
  };
  for (size_t i = 0; i < sizeof table / sizeof table[0]; ++i)
    if (table[i].errno_value == e) return table[i].dos;
  return XB_FERR_FAILURE;
}

/* The DOS error number of a failure on `path`: a missing directory on the way is "path not found" (3), as on
 * Windows, where errno says ENOENT whichever part is missing. */
static int32_t path_error(const char *path, int e) {
  const char *slash = strrchr(path, '/');
  if (e != ENOENT || !slash) return dos_error(e);
  size_t len = slash == path ? 1 : (size_t)(slash - path); /* "/name" is in the root */
  char *dir = malloc(len + 1);
  if (!dir) return dos_error(e);
  memcpy(dir, path, len);
  dir[len] = '\0';
  struct stat st;
  int found = stat(dir, &st) == 0 && S_ISDIR(st.st_mode);
  free(dir);
  return found ? dos_error(e) : 3;
}

static int open_as(const char *path, int flags, mode_t perms, xb_file *f, int32_t *err) {
  int fd;
  do {
    fd = open(path, flags | O_CLOEXEC, perms);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) {
    *err = path_error(path, errno);
    return 0;
  }
  *f = fd;
  return 1;
}

int xb_platform_file_open(const char *path, int32_t mode, xb_file *f, int32_t *err) {
  static const int access[] = {O_RDONLY, O_WRONLY, O_RDWR};
  if (!open_as(path, access[mode & (FO_WRITE | FO_READWRITE)], 0, f, err)) return 0;
  struct stat st;
  if (fstat((int)*f, &st) == 0 && S_ISDIR(st.st_mode)) { /* a directory opens for reading on POSIX systems only */
    close((int)*f);
    *err = XB_FERR_ACCESS;
    return 0;
  }
  return 1;
}

int xb_platform_file_open_always(const char *path, int32_t mode, xb_file *f, int32_t *err) {
  (void)mode; /* the sharing is xb_platform_file_share's on POSIX systems */
  return open_as(path, O_RDWR | O_CREAT, 0666, f, err);
}

int xb_platform_file_create(const char *path, int32_t attr, xb_file *f, int32_t *err) {
  return open_as(path, O_RDWR | O_CREAT | O_TRUNC, attr & XB_FA_READONLY ? 0444 : 0666, f, err);
}

int64_t xb_platform_file_read(xb_file f, void *buf, size_t n, int32_t *err) {
  ssize_t got;
  do {
    got = read((int)f, buf, n);
  } while (got < 0 && errno == EINTR);
  if (got < 0) *err = dos_error(errno);
  return got;
}

int64_t xb_platform_file_read_at(xb_file f, int64_t pos, void *buf, size_t n, int32_t *err) {
  ssize_t got;
  do {
    got = pread((int)f, buf, n, (off_t)pos);
  } while (got < 0 && errno == EINTR);
  if (got < 0) *err = dos_error(errno);
  return got;
}

/* Writes all `n` bytes at `pos` (-1: at the file's position). */
static int write_all(xb_file f, int64_t pos, const void *buf, size_t n, int32_t *err) {
  const char *p = buf;
  while (n > 0) {
    ssize_t put = pos < 0 ? write((int)f, p, n) : pwrite((int)f, p, n, (off_t)pos);
    if (put < 0 && errno == EINTR) continue;
    if (put <= 0) {
      *err = put < 0 ? dos_error(errno) : XB_FERR_DISK_FULL; /* nothing written: no room */
      return 0;
    }
    p += put;
    if (pos >= 0) pos += put;
    n -= (size_t)put;
  }
  return 1;
}

int xb_platform_file_write_at(xb_file f, int64_t pos, const void *buf, size_t n, int32_t *err) {
  return write_all(f, pos, buf, n, err);
}

int xb_platform_file_write(xb_file f, const void *buf, size_t n, int32_t *err) {
  return write_all(f, -1, buf, n, err);
}

int64_t xb_platform_file_seek(xb_file f, int64_t offset, int origin, int32_t *err) {
  static const int whence[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  if ((off_t)offset != offset) { /* past what this system's offsets hold */
    *err = 25;
    return -1;
  }
  off_t pos = lseek((int)f, (off_t)offset, whence[origin]);
  if (pos < 0) *err = errno == EINVAL ? 25 : dos_error(errno);
  return pos;
}

int xb_platform_file_share(xb_file f, int kind, int32_t *err) {
  int how = kind == XB_LOCK_WRITE ? LOCK_EX | LOCK_NB : kind == XB_LOCK_READ ? LOCK_SH | LOCK_NB : LOCK_UN;
  int r;
  do {
    r = flock((int)f, how);
  } while (r != 0 && errno == EINTR);
  if (r == 0) return 1;
  *err = errno == EWOULDBLOCK ? 32 : dos_error(errno);
  return 0;
}

int xb_platform_file_lock(xb_file f, uint64_t pos, uint64_t len, int kind, int32_t *err) {
  if ((uint64_t)(off_t)pos != pos || (uint64_t)(off_t)len != len) {
    *err = 87;
    return 0;
  }
  struct flock l;
  memset(&l, 0, sizeof l);
  l.l_type = (short)(kind == XB_LOCK_WRITE ? F_WRLCK : kind == XB_LOCK_READ ? F_RDLCK : F_UNLCK);
  l.l_whence = SEEK_SET;
  l.l_start = (off_t)pos;
  l.l_len = (off_t)len;
  int r;
  do {
    r = fcntl((int)f, F_SETLK, &l);
  } while (r != 0 && errno == EINTR);
  if (r == 0) return 1;
  *err = errno == EACCES || errno == EAGAIN ? XB_FERR_LOCKED : dos_error(errno);
  return 0;
}

int xb_platform_locks_per_handle(void) { return 0; }

int xb_platform_file_id(xb_file f, xb_file_id *id, int32_t *err) {
  struct stat st;
  if (fstat((int)f, &st) != 0) {
    *err = dos_error(errno);
    return 0;
  }
  id->volume = (uint64_t)st.st_dev;
  id->index = (uint64_t)st.st_ino;
  return 1;
}

int xb_platform_path_id(const char *path, xb_file_id *id, int32_t *err) {
  struct stat st;
  if (stat(path, &st) != 0) {
    *err = path_error(path, errno);
    return 0;
  }
  id->volume = (uint64_t)st.st_dev;
  id->index = (uint64_t)st.st_ino;
  return 1;
}

void xb_platform_sleep_ms(uint32_t ms) {
  struct timespec t = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000L};
  if (ms == 0) t.tv_nsec = 1000; /* the shortest pause the clock gives */
  while (nanosleep(&t, &t) != 0 && errno == EINTR) {
  }
}

int xb_platform_file_truncate(xb_file f, int64_t size, int32_t *err) {
  if ((off_t)size != size) {
    *err = 25;
    return 0;
  }
  int r;
  do {
    r = ftruncate((int)f, (off_t)size);
  } while (r != 0 && errno == EINTR);
  if (r == 0) return 1;
  *err = dos_error(errno);
  return 0;
}

int xb_platform_file_close(xb_file f, int32_t *err) {
  if (close((int)f) == 0 || errno == EINTR) return 1; /* after EINTR the descriptor is closed, as on Linux */
  *err = dos_error(errno);
  return 0;
}

int xb_platform_file_erase(const char *path, int32_t *err) {
  struct stat st;
  if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
    *err = XB_FERR_ACCESS;
    return 0;
  }
  if (unlink(path) == 0) return 1;
  *err = path_error(path, errno);
  return 0;
}

int xb_platform_file_rename(const char *from, const char *to, int32_t *err) {
  struct stat target, source;
  if (lstat(to, &target) == 0 &&
      (lstat(from, &source) != 0 || source.st_dev != target.st_dev || source.st_ino != target.st_ino)) {
    *err = XB_FERR_ACCESS;
    return 0;
  }
  if (rename(from, to) == 0) return 1;
  int e = errno;
  *err = path_error(from, e);
  if (*err != 3) *err = path_error(to, e);
  return 0;
}

/* The entry of a file whose status is `st` and whose name is `name`. */
static void fill_entry(xb_dir_entry *e, const char *name, const struct stat *st) {
  e->name = name;
  e->size = S_ISREG(st->st_mode) ? (int64_t)st->st_size : 0;
  e->attrs = 0;
  if (S_ISDIR(st->st_mode)) e->attrs |= XB_FA_DIRECTORY;
  if (S_ISREG(st->st_mode)) e->attrs |= XB_FA_ARCHIVE;
  if (!(st->st_mode & S_IWUSR)) e->attrs |= XB_FA_READONLY;
  if (name[0] == '.' && strcmp(name, ".") != 0 && strcmp(name, "..") != 0) e->attrs |= XB_FA_HIDDEN;
  struct tm tm;
  time_t t = st->st_mtime;
  localtime_r(&t, &tm);
  e->year = tm.tm_year + 1900;
  e->month = tm.tm_mon + 1;
  e->day = tm.tm_mday;
  e->hour = tm.tm_hour;
  e->minute = tm.tm_min;
  e->second = tm.tm_sec;
}

/* The status of a path, or of the link itself when it leads nowhere. */
static int status_of(const char *path, struct stat *st) { return stat(path, st) == 0 || lstat(path, st) == 0; }

int xb_platform_file_info(const char *path, const char *name, xb_dir_entry *e, int32_t *err) {
  struct stat st;
  if (!status_of(path, &st)) {
    *err = path_error(path, errno);
    return 0;
  }
  fill_entry(e, name, &st);
  return 1;
}

struct xb_dir_impl {
  DIR *dir;
  char *path; /* the directory, then the name of the entry being read */
  size_t dirLen, cap;
};

int xb_platform_dir_open(const char *path, xb_dir *d, int32_t *err) {
  const char *dirPath = path[0] ? path : ".";
  DIR *dir = opendir(dirPath);
  if (!dir) {
    *err = dos_error(errno);
    return 0;
  }
  xb_dir out = malloc(sizeof *out);
  size_t len = strlen(dirPath), cap = len + 2; /* the directory, a separator and the terminator */
  char *buf = out ? malloc(cap) : NULL;
  if (!buf) {
    free(out);
    closedir(dir);
    *err = 8;
    return 0;
  }
  memcpy(buf, dirPath, len);
  if (buf[len - 1] != '/') buf[len++] = '/';
  buf[len] = '\0';
  out->dir = dir;
  out->path = buf;
  out->dirLen = len;
  out->cap = cap;
  *d = out;
  return 1;
}

int xb_platform_dir_next(xb_dir d, xb_dir_entry *e) {
  for (;;) {
    struct dirent *de = readdir(d->dir);
    if (!de) return 0;
    size_t len = strlen(de->d_name);
    if (d->dirLen + len + 1 > d->cap) {
      char *grown = realloc(d->path, d->dirLen + len + 1);
      if (!grown) return 0;
      d->path = grown;
      d->cap = d->dirLen + len + 1;
    }
    memcpy(d->path + d->dirLen, de->d_name, len + 1);
    struct stat st;
    if (status_of(d->path, &st)) { /* one that vanished since it was listed is left out */
      fill_entry(e, d->path + d->dirLen, &st);
      return 1;
    }
  }
}

void xb_platform_dir_close(xb_dir d) {
  closedir(d->dir);
  free(d->path);
  free(d);
}

int xb_platform_names_case_sensitive(const char *dir) {
#if defined(_PC_CASE_SENSITIVE)
  return pathconf(dir[0] ? dir : ".", _PC_CASE_SENSITIVE) != 0;
#else
  (void)dir;
  return 1;
#endif
}

char *xb_platform_cwd(int32_t *err) {
  for (size_t cap = 256;; cap *= 2) {
    char *buf = malloc(cap);
    if (!buf) {
      *err = 8;
      return NULL;
    }
    if (getcwd(buf, cap)) return buf;
    free(buf);
    if (errno != ERANGE) {
      *err = dos_error(errno);
      return NULL;
    }
  }
}

int xb_platform_chdir(const char *path, int32_t *err) {
  if (chdir(path) == 0) return 1;
  *err = dos_error(errno);
  return 0;
}

int xb_platform_disk_free(int drive, int64_t *bytes, int32_t *err) {
  struct statvfs fs;
  (void)drive;
  if (statvfs(".", &fs) != 0) {
    *err = dos_error(errno);
    return 0;
  }
  *bytes = (int64_t)fs.f_bavail * (int64_t)fs.f_frsize;
  return 1;
}

char *xb_platform_getenv(const char *name) {
  const char *v = getenv(name);
  if (!v) return NULL;
  size_t len = strlen(v);
  char *copy = malloc(len + 1);
  if (copy) memcpy(copy, v, len + 1);
  return copy;
}
