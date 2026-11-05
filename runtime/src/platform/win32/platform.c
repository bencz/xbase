#include "platform/platform.h"

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#include <shellapi.h>

static LARGE_INTEGER started, frequency;

/* Program output is byte-exact on every platform: no CRLF translation. */
void xb_platform_init(void) {
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&started);
}

/* The counter's frequency is fixed at boot; whole seconds and the remainder
 * are scaled apart, so the product cannot overflow. */
int64_t xb_platform_elapsed_ms(void) {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  int64_t ticks = now.QuadPart - started.QuadPart, hz = frequency.QuadPart;
  return ticks / hz * 1000 + ticks % hz * 1000 / hz;
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

int xb_mutex_init(xb_mutex *m) {
  *m = malloc(sizeof **m);
  if (!*m) return 0;
  InitializeSRWLock(&(*m)->lock);
  return 1;
}

void xb_mutex_lock(xb_mutex *m) { AcquireSRWLockExclusive(&(*m)->lock); }
void xb_mutex_unlock(xb_mutex *m) { ReleaseSRWLockExclusive(&(*m)->lock); }

void xb_platform_local_time(int *hour, int *minute, int *second, int *msec) {
  SYSTEMTIME st;
  GetLocalTime(&st);
  *hour = st.wHour;
  *minute = st.wMinute;
  *second = st.wSecond;
  *msec = st.wMilliseconds;
}

void xb_platform_local_date(int *year, int *month, int *day) {
  time_t now = time(NULL);
  struct tm tm;
  localtime_s(&tm, &now);
  *year = tm.tm_year + 1900;
  *month = tm.tm_mon + 1;
  *day = tm.tm_mday;
}

/* main() receives the arguments in the ANSI code page; the wide command line
 * converted to UTF-8 keeps every character. On failure, main()'s array. */
char **xb_platform_args(int argc, char **argv, int *count) {
  int n = 0;
  LPWSTR *wide = CommandLineToArgvW(GetCommandLineW(), &n);
  char **out = wide ? calloc((size_t)n + 1, sizeof *out) : NULL;
  for (int i = 0; out && i < n; ++i) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, NULL, 0, NULL, NULL);
    out[i] = len > 0 ? malloc((size_t)len) : NULL;
    if (!out[i] || WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, out[i], len, NULL, NULL) != len) out = NULL;
  }
  if (wide) LocalFree(wide);
  if (!out) {
    *count = argc;
    return argv;
  }
  *count = n;
  return out;
}
