/* The POSIX interfaces this file uses (clock_gettime, localtime_r, threads,
 * files), which a strict C11 build declares only on request. */
#define _POSIX_C_SOURCE 200809L

#include "platform/platform.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
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
  return 31;
}

static int open_as(const char *path, int flags, xb_file *f, int32_t *err) {
  int fd;
  do {
    fd = open(path, flags | O_CLOEXEC, 0666);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) {
    *err = dos_error(errno);
    return 0;
  }
  *f = fd;
  return 1;
}

int xb_platform_file_open(const char *path, xb_file *f, int32_t *err) { return open_as(path, O_RDONLY, f, err); }

int xb_platform_file_create(const char *path, xb_file *f, int32_t *err) {
  return open_as(path, O_WRONLY | O_CREAT | O_TRUNC, f, err);
}

int64_t xb_platform_file_read(xb_file f, void *buf, size_t n, int32_t *err) {
  ssize_t got;
  do {
    got = read((int)f, buf, n);
  } while (got < 0 && errno == EINTR);
  if (got < 0) *err = dos_error(errno);
  return got;
}

int xb_platform_file_write(xb_file f, const void *buf, size_t n, int32_t *err) {
  const char *p = buf;
  while (n > 0) {
    ssize_t put = write((int)f, p, n);
    if (put < 0 && errno == EINTR) continue;
    if (put <= 0) {
      *err = put < 0 ? dos_error(errno) : 112; /* nothing written: no room */
      return 0;
    }
    p += put;
    n -= (size_t)put;
  }
  return 1;
}

int xb_platform_file_close(xb_file f, int32_t *err) {
  if (close((int)f) == 0 || errno == EINTR) return 1; /* after EINTR the descriptor is closed, as on Linux */
  *err = dos_error(errno);
  return 0;
}
