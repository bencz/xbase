#include "platform/platform.h"

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

/* Program output is byte-exact on every platform: no CRLF translation. */
void xb_platform_init(void) {
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
}

struct xb_mutex_impl {
  SRWLOCK lock;
};

void xb_mutex_init(xb_mutex *m) {
  *m = malloc(sizeof **m);
  if (!*m) abort();
  InitializeSRWLock(&(*m)->lock);
}

void xb_mutex_lock(xb_mutex *m) { AcquireSRWLockExclusive(&(*m)->lock); }
void xb_mutex_unlock(xb_mutex *m) { ReleaseSRWLockExclusive(&(*m)->lock); }

void xb_platform_local_date(int *year, int *month, int *day) {
  time_t now = time(NULL);
  struct tm tm;
  localtime_s(&tm, &now);
  *year = tm.tm_year + 1900;
  *month = tm.tm_mon + 1;
  *day = tm.tm_mday;
}
