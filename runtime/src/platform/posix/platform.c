#include "platform/platform.h"

#include <pthread.h>
#include <stdlib.h>
#include <time.h>

void xb_platform_init(void) {}

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

void xb_mutex_init(xb_mutex *m) {
  *m = malloc(sizeof **m);
  if (!*m || pthread_mutex_init(&(*m)->m, NULL) != 0) abort();
}

void xb_mutex_lock(xb_mutex *m) { pthread_mutex_lock(&(*m)->m); }
void xb_mutex_unlock(xb_mutex *m) { pthread_mutex_unlock(&(*m)->m); }

void xb_platform_local_date(int *year, int *month, int *day) {
  time_t now = time(NULL);
  struct tm tm;
  localtime_r(&now, &tm);
  *year = tm.tm_year + 1900;
  *month = tm.tm_mon + 1;
  *day = tm.tm_mday;
}
