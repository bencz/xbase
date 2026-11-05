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

typedef struct {
  void (*fn)(void *);
  void *arg;
} thread_start;

static DWORD WINAPI run_thread(LPVOID p) {
  thread_start *start = p;
  start->fn(start->arg);
  return 0;
}

int xb_platform_run_on_stack(size_t stack_size, void (*fn)(void *), void *arg) {
  thread_start start = {fn, arg};
  HANDLE thread = CreateThread(NULL, stack_size, run_thread, &start, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
  if (!thread) return 0;
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
  return 1;
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
