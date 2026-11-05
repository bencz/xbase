#include "platform/platform.h"

#include <pthread.h>
#include <stdlib.h>

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
