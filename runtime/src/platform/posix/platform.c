#include "platform/platform.h"

#include <pthread.h>
#include <stdlib.h>
#include <time.h>

void xb_platform_init(void) {}

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
