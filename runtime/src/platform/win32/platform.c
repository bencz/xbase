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

int xb_platform_path_separator(char c) { return c == '/' || c == '\\' || c == ':'; }

/* The UTF-8 path as UTF-16 (malloc'ed), or NULL with the error code. */
static wchar_t *wide_path(const char *path, int32_t *err) {
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
  if (n <= 0) { /* not UTF-8 */
    *err = ERROR_INVALID_NAME;
    return NULL;
  }
  wchar_t *w = malloc((size_t)n * sizeof *w);
  if (!w) {
    *err = ERROR_NOT_ENOUGH_MEMORY;
    return NULL;
  }
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, w, n);
  return w;
}

static int open_as(const char *path, DWORD access, DWORD share, DWORD disposition, xb_file *f, int32_t *err) {
  wchar_t *w = wide_path(path, err);
  if (!w) return 0;
  HANDLE h = CreateFileW(w, access, share, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
  DWORD code = GetLastError();
  free(w);
  if (h == INVALID_HANDLE_VALUE) {
    *err = (int32_t)code;
    return 0;
  }
  *f = (xb_file)h;
  return 1;
}

int xb_platform_file_open(const char *path, xb_file *f, int32_t *err) {
  return open_as(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, f, err);
}

int xb_platform_file_create(const char *path, xb_file *f, int32_t *err) {
  return open_as(path, GENERIC_WRITE, 0, CREATE_ALWAYS, f, err);
}

/* ReadFile and WriteFile move at most this much at a time (a DWORD count). */
#define FILE_CHUNK ((DWORD)1 << 30)

int64_t xb_platform_file_read(xb_file f, void *buf, size_t n, int32_t *err) {
  DWORD got = 0;
  if (!ReadFile((HANDLE)f, buf, n < FILE_CHUNK ? (DWORD)n : FILE_CHUNK, &got, NULL)) {
    *err = (int32_t)GetLastError();
    return -1;
  }
  return got;
}

int xb_platform_file_write(xb_file f, const void *buf, size_t n, int32_t *err) {
  const char *p = buf;
  while (n > 0) {
    DWORD put = 0;
    if (!WriteFile((HANDLE)f, p, n < FILE_CHUNK ? (DWORD)n : FILE_CHUNK, &put, NULL)) {
      *err = (int32_t)GetLastError();
      return 0;
    }
    if (put == 0) {
      *err = ERROR_DISK_FULL;
      return 0;
    }
    p += put;
    n -= put;
  }
  return 1;
}

int xb_platform_file_close(xb_file f, int32_t *err) {
  if (CloseHandle((HANDLE)f)) return 1;
  *err = (int32_t)GetLastError();
  return 0;
}
