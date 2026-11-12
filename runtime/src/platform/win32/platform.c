#include "platform/platform.h"

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include <shellapi.h>

/* Program output is byte-exact on every platform: no CRLF translation. */
void xb_platform_init(void) {
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
}

/* The counter's frequency is fixed at boot (asked each time, which costs a read of memory the system shares); whole
 * seconds and the remainder are scaled apart, so the product cannot overflow. */
int64_t xb_platform_monotonic_ns(void) {
  LARGE_INTEGER now, frequency;
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&now);
  int64_t ticks = now.QuadPart, hz = frequency.QuadPart;
  return ticks / hz * XB_NS_PER_SEC + ticks % hz * XB_NS_PER_SEC / hz;
}

/* The low limit is the reservation's start, its guard pages included: the stack error's reserve lies above them. */
const char *xb_platform_stack_low(void) {
  ULONG_PTR low = 0, high = 0;
  GetCurrentThreadStackLimits(&low, &high);
  return low ? (const char *)low : NULL;
}

unsigned xb_platform_cpu_count(void) {
  DWORD n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  return n ? (unsigned)n : 1u;
}

struct xb_thread_impl {
  HANDLE thread;
  void (*fn)(void *);
  void *arg;
};

static DWORD WINAPI run_thread(LPVOID p) {
  xb_thread t = p;
  t->fn(t->arg);
  return 0;
}

int xb_thread_start(xb_thread *t, size_t stack_size, void (*fn)(void *), void *arg) {
  *t = malloc(sizeof **t);
  if (!*t) return 0;
  (*t)->fn = fn;
  (*t)->arg = arg;
  (*t)->thread = CreateThread(NULL, stack_size, run_thread, *t, stack_size ? STACK_SIZE_PARAM_IS_A_RESERVATION : 0,
                              NULL);
  if ((*t)->thread) return 1;
  free(*t);
  *t = NULL;
  return 0;
}

void xb_thread_join(xb_thread t) {
  WaitForSingleObject(t->thread, INFINITE);
  CloseHandle(t->thread);
  free(t);
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

void xb_mutex_destroy(xb_mutex *m) { free(*m); }
void xb_mutex_lock(xb_mutex *m) { AcquireSRWLockExclusive(&(*m)->lock); }
int xb_mutex_trylock(xb_mutex *m) { return TryAcquireSRWLockExclusive(&(*m)->lock) != 0; }
void xb_mutex_unlock(xb_mutex *m) { ReleaseSRWLockExclusive(&(*m)->lock); }

struct xb_cond_impl {
  CONDITION_VARIABLE c;
};

int xb_cond_init(xb_cond *c) {
  *c = malloc(sizeof **c);
  if (!*c) return 0;
  InitializeConditionVariable(&(*c)->c);
  return 1;
}

void xb_cond_destroy(xb_cond *c) { free(*c); }
void xb_cond_wait(xb_cond *c, xb_mutex *m) { SleepConditionVariableSRW(&(*c)->c, &(*m)->lock, INFINITE, 0); }

/* The system waits in whole milliseconds, rounded up so that the wait is never shorter than asked, and for less than
 * INFINITE: a longer wait that ends is no timeout (the waiter tests its condition, and waits again). */
int xb_cond_wait_ns(xb_cond *c, xb_mutex *m, int64_t ns) {
  if (ns > XB_WAIT_NS_MAX) ns = XB_WAIT_NS_MAX;
  int64_t ms = ns <= 0 ? 0 : (ns + XB_NS_PER_MS - 1) / XB_NS_PER_MS;
  int cut = ms >= INFINITE;
  if (cut) ms = INFINITE - 1;
  return SleepConditionVariableSRW(&(*c)->c, &(*m)->lock, (DWORD)ms, 0) || cut || GetLastError() != ERROR_TIMEOUT;
}

void xb_cond_signal(xb_cond *c) { WakeConditionVariable(&(*c)->c); }
void xb_cond_broadcast(xb_cond *c) { WakeAllConditionVariable(&(*c)->c); }

struct xb_rwlock_impl {
  SRWLOCK lock;
};

int xb_rwlock_init(xb_rwlock *l) {
  *l = malloc(sizeof **l);
  if (!*l) return 0;
  InitializeSRWLock(&(*l)->lock);
  return 1;
}

void xb_rwlock_destroy(xb_rwlock *l) { free(*l); }
void xb_rwlock_read_lock(xb_rwlock *l) { AcquireSRWLockShared(&(*l)->lock); }
void xb_rwlock_read_unlock(xb_rwlock *l) { ReleaseSRWLockShared(&(*l)->lock); }
void xb_rwlock_write_lock(xb_rwlock *l) { AcquireSRWLockExclusive(&(*l)->lock); }
void xb_rwlock_write_unlock(xb_rwlock *l) { ReleaseSRWLockExclusive(&(*l)->lock); }

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

const char *xb_platform_eol(void) { return "\r\n"; }

void xb_platform_with_registers(void (*fn)(void *), void *arg) {
  CONTEXT registers; /* in this frame, which the scan from inside fn covers */
  volatile int returned = 0;
  RtlCaptureContext(&registers);
  fn(arg);
  returned = 1; /* a store after the call: fn(arg) is no tail call, which would free the frame first */
  (void)returned;
}

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

/* UTF-16 as UTF-8 (malloc'ed), or NULL. */
static char *utf8_of(const wchar_t *w) {
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
  char *out = n > 0 ? malloc((size_t)n) : NULL;
  if (out && WideCharToMultiByte(CP_UTF8, 0, w, -1, out, n, NULL, NULL) != n) {
    free(out);
    out = NULL;
  }
  return out;
}

/* The codes the platform layer gives the same on every system (platform.h). */
static int32_t common_code(DWORD code) {
  if (code == ERROR_NEGATIVE_SEEK) return 25;
  if (code == ERROR_ALREADY_EXISTS) return 5;
  return (int32_t)code;
}

static int open_as(const char *path, DWORD access, DWORD share, DWORD disposition, DWORD attrs, xb_file *f,
                   int32_t *err) {
  wchar_t *w = wide_path(path, err);
  if (!w) return 0;
  HANDLE h = CreateFileW(w, access, share, NULL, disposition, attrs, NULL);
  DWORD code = GetLastError();
  free(w);
  if (h == INVALID_HANDLE_VALUE) {
    *err = common_code(code);
    return 0;
  }
  *f = (xb_file)h;
  return 1;
}

/* The share mode of an FOpen() mode's sharing. */
static DWORD share_of(int32_t mode) {
  switch (mode & (FO_EXCLUSIVE | FO_DENYWRITE | FO_DENYREAD | FO_DENYNONE)) {
  case FO_EXCLUSIVE: return 0;
  case FO_DENYWRITE: return FILE_SHARE_READ;
  case FO_DENYREAD: return FILE_SHARE_WRITE;
  default: return FILE_SHARE_READ | FILE_SHARE_WRITE; /* compatibility mode and deny none */
  }
}

int xb_platform_file_open(const char *path, int32_t mode, xb_file *f, int32_t *err) {
  static const DWORD access[] = {GENERIC_READ, GENERIC_WRITE, GENERIC_READ | GENERIC_WRITE};
  return open_as(path, access[mode & (FO_WRITE | FO_READWRITE)], share_of(mode), OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                 f, err);
}

int xb_platform_file_open_always(const char *path, int32_t mode, xb_file *f, int32_t *err) {
  return open_as(path, GENERIC_READ | GENERIC_WRITE, share_of(mode), OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, f, err);
}

int xb_platform_file_create(const char *path, int32_t attr, xb_file *f, int32_t *err) {
  DWORD attrs = (DWORD)attr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
  wchar_t *w = wide_path(path, err);
  if (!w) return 0;
  DWORD existing = GetFileAttributesW(w); /* CREATE_ALWAYS refuses a hidden or system file unless asked to keep it */
  free(w);
  if (existing != INVALID_FILE_ATTRIBUTES) attrs |= existing & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
  return open_as(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, CREATE_ALWAYS,
                 attrs ? attrs : FILE_ATTRIBUTE_NORMAL, f, err);
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


/* An OVERLAPPED that gives a synchronous handle's read or write its offset. */
static OVERLAPPED at_offset(int64_t pos) {
  OVERLAPPED o;
  memset(&o, 0, sizeof o);
  o.Offset = (DWORD)((uint64_t)pos & 0xFFFFFFFFu);
  o.OffsetHigh = (DWORD)((uint64_t)pos >> 32);
  return o;
}

int64_t xb_platform_file_read_at(xb_file f, int64_t pos, void *buf, size_t n, int32_t *err) {
  OVERLAPPED o = at_offset(pos);
  DWORD got = 0;
  if (!ReadFile((HANDLE)f, buf, n < FILE_CHUNK ? (DWORD)n : FILE_CHUNK, &got, &o)) {
    DWORD e = GetLastError();
    if (e == ERROR_HANDLE_EOF) return 0;
    *err = (int32_t)e;
    return -1;
  }
  return got;
}

/* Writes all `n` bytes at `pos` (-1: at the file's position). */
static int write_all(xb_file f, int64_t pos, const void *buf, size_t n, int32_t *err) {
  const char *p = buf;
  while (n > 0) {
    OVERLAPPED o = at_offset(pos);
    DWORD put = 0;
    if (!WriteFile((HANDLE)f, p, n < FILE_CHUNK ? (DWORD)n : FILE_CHUNK, &put, pos < 0 ? NULL : &o)) {
      *err = (int32_t)GetLastError();
      return 0;
    }
    if (put == 0) {
      *err = ERROR_DISK_FULL;
      return 0;
    }
    p += put;
    if (pos >= 0) pos += put;
    n -= put;
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
  static const DWORD method[] = {FILE_BEGIN, FILE_CURRENT, FILE_END};
  LARGE_INTEGER to, pos;
  to.QuadPart = offset;
  if (!SetFilePointerEx((HANDLE)f, to, &pos, method[origin])) {
    *err = common_code(GetLastError());
    return -1;
  }
  return pos.QuadPart;
}

int xb_platform_file_share(xb_file f, int kind, int32_t *err) {
  (void)f;
  (void)kind;
  (void)err;
  return 1;
}

int xb_platform_file_lock(xb_file f, uint64_t pos, uint64_t len, int kind, int32_t *err) {
  OVERLAPPED o;
  memset(&o, 0, sizeof o);
  o.Offset = (DWORD)(pos & 0xFFFFFFFFu);
  o.OffsetHigh = (DWORD)(pos >> 32);
  DWORD low = (DWORD)(len & 0xFFFFFFFFu), high = (DWORD)(len >> 32);
  BOOL ok;
  if (kind == XB_LOCK_NONE) {
    ok = UnlockFileEx((HANDLE)f, 0, low, high, &o);
  } else {
    DWORD flags = (kind == XB_LOCK_WRITE ? LOCKFILE_EXCLUSIVE_LOCK : 0) | LOCKFILE_FAIL_IMMEDIATELY;
    ok = LockFileEx((HANDLE)f, flags, 0, low, high, &o);
  }
  if (ok) return 1;
  DWORD code = GetLastError();
  *err = code == ERROR_LOCK_VIOLATION || code == ERROR_IO_PENDING ? XB_FERR_LOCKED : common_code(code);
  return 0;
}

int xb_platform_locks_per_handle(void) { return 1; }

int xb_platform_file_id(xb_file f, xb_file_id *id, int32_t *err) {
  BY_HANDLE_FILE_INFORMATION info;
  if (!GetFileInformationByHandle((HANDLE)f, &info)) {
    *err = common_code(GetLastError());
    return 0;
  }
  id->volume = info.dwVolumeSerialNumber;
  id->index = (uint64_t)info.nFileIndexHigh << 32 | info.nFileIndexLow;
  return 1;
}

int xb_platform_path_id(const char *path, xb_file_id *id, int32_t *err) {
  wchar_t *w = wide_path(path, err);
  if (!w) return 0;
  HANDLE h = CreateFileW(w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS, NULL);
  DWORD code = GetLastError();
  free(w);
  if (h == INVALID_HANDLE_VALUE) {
    *err = common_code(code);
    return 0;
  }
  int ok = xb_platform_file_id((xb_file)h, id, err);
  CloseHandle(h); /* a handle with no access: it holds no lock */
  return ok;
}

void xb_platform_sleep_ms(uint32_t ms) { Sleep(ms); }

int xb_platform_file_truncate(xb_file f, int64_t size, int32_t *err) {
  LARGE_INTEGER zero, here, to;
  zero.QuadPart = 0;
  to.QuadPart = size;
  if (!SetFilePointerEx((HANDLE)f, zero, &here, FILE_CURRENT) || !SetFilePointerEx((HANDLE)f, to, NULL, FILE_BEGIN) ||
      !SetEndOfFile((HANDLE)f)) {
    *err = common_code(GetLastError());
    return 0;
  }
  SetFilePointerEx((HANDLE)f, here, NULL, FILE_BEGIN);
  return 1;
}

int xb_platform_file_close(xb_file f, int32_t *err) {
  if (CloseHandle((HANDLE)f)) return 1;
  *err = (int32_t)GetLastError();
  return 0;
}

int xb_platform_file_erase(const char *path, int32_t *err) {
  wchar_t *w = wide_path(path, err);
  if (!w) return 0;
  BOOL ok = DeleteFileW(w); /* a directory is ERROR_ACCESS_DENIED */
  DWORD code = GetLastError();
  free(w);
  if (!ok) *err = common_code(code);
  return ok != 0;
}

int xb_platform_file_rename(const char *from, const char *to, int32_t *err) {
  wchar_t *wf = wide_path(from, err);
  if (!wf) return 0;
  wchar_t *wt = wide_path(to, err);
  if (!wt) {
    free(wf);
    return 0;
  }
  BOOL ok = MoveFileW(wf, wt); /* never replaces: an existing name is ERROR_ALREADY_EXISTS */
  DWORD code = GetLastError();
  free(wf);
  free(wt);
  if (!ok) *err = common_code(code);
  return ok != 0;
}

/* The fields of an entry from its attributes, size and time of last change. */
static void fill_entry(xb_dir_entry *e, DWORD attrs, DWORD sizeHigh, DWORD sizeLow, const FILETIME *written) {
  e->attrs = attrs & (XB_FA_READONLY | XB_FA_HIDDEN | XB_FA_SYSTEM | XB_FA_DIRECTORY | XB_FA_ARCHIVE);
  e->size = attrs & FILE_ATTRIBUTE_DIRECTORY ? 0 : (int64_t)(((uint64_t)sizeHigh << 32) | sizeLow);
  SYSTEMTIME utc, local;
  if (!FileTimeToSystemTime(written, &utc) || !SystemTimeToTzSpecificLocalTime(NULL, &utc, &local))
    memset(&local, 0, sizeof local);
  e->year = local.wYear;
  e->month = local.wMonth;
  e->day = local.wDay;
  e->hour = local.wHour;
  e->minute = local.wMinute;
  e->second = local.wSecond;
}

int xb_platform_file_info(const char *path, const char *name, xb_dir_entry *e, int32_t *err) {
  wchar_t *w = wide_path(path, err);
  if (!w) return 0;
  WIN32_FILE_ATTRIBUTE_DATA data;
  BOOL ok = GetFileAttributesExW(w, GetFileExInfoStandard, &data);
  DWORD code = GetLastError();
  free(w);
  if (!ok) {
    *err = common_code(code);
    return 0;
  }
  e->name = name;
  fill_entry(e, data.dwFileAttributes, data.nFileSizeHigh, data.nFileSizeLow, &data.ftLastWriteTime);
  return 1;
}

struct xb_dir_impl {
  HANDLE find;
  WIN32_FIND_DATAW data;
  int pending; /* `data` holds an entry not given yet (the first one) */
  char *name;  /* the last entry's name */
};

int xb_platform_dir_open(const char *path, xb_dir *d, int32_t *err) {
  size_t len = strlen(path);
  char *pattern = malloc(len + 3);
  if (!pattern) {
    *err = ERROR_NOT_ENOUGH_MEMORY;
    return 0;
  }
  memcpy(pattern, path, len);
  if (len && !xb_platform_path_separator(path[len - 1])) pattern[len++] = '\\';
  pattern[len++] = '*';
  pattern[len] = '\0';
  wchar_t *w = wide_path(pattern, err);
  free(pattern);
  xb_dir out = w ? calloc(1, sizeof *out) : NULL;
  if (!out) {
    if (w) *err = ERROR_NOT_ENOUGH_MEMORY;
    free(w);
    return 0;
  }
  out->find = FindFirstFileW(w, &out->data);
  DWORD code = GetLastError();
  free(w);
  if (out->find == INVALID_HANDLE_VALUE) {
    free(out);
    *err = common_code(code);
    return 0;
  }
  out->pending = 1;
  *d = out;
  return 1;
}

int xb_platform_dir_next(xb_dir d, xb_dir_entry *e) {
  if (!d->pending && !FindNextFileW(d->find, &d->data)) return 0;
  d->pending = 0;
  free(d->name);
  d->name = utf8_of(d->data.cFileName);
  if (!d->name) return 0;
  e->name = d->name;
  fill_entry(e, d->data.dwFileAttributes, d->data.nFileSizeHigh, d->data.nFileSizeLow, &d->data.ftLastWriteTime);
  return 1;
}

void xb_platform_dir_close(xb_dir d) {
  FindClose(d->find);
  free(d->name);
  free(d);
}

int xb_platform_names_case_sensitive(const char *dir) {
  (void)dir;
  return 0;
}

char *xb_platform_cwd(int32_t *err) {
  DWORD n = GetCurrentDirectoryW(0, NULL);
  wchar_t *w = n ? malloc(n * sizeof *w) : NULL;
  if (!w || GetCurrentDirectoryW(n, w) == 0) {
    *err = w ? (int32_t)GetLastError() : ERROR_NOT_ENOUGH_MEMORY;
    free(w);
    return NULL;
  }
  char *out = utf8_of(w);
  free(w);
  if (!out) *err = ERROR_NOT_ENOUGH_MEMORY;
  return out;
}

int xb_platform_chdir(const char *path, int32_t *err) {
  wchar_t *w = wide_path(path, err);
  if (!w) return 0;
  BOOL ok = SetCurrentDirectoryW(w);
  DWORD code = GetLastError();
  free(w);
  if (!ok) *err = common_code(code);
  return ok != 0;
}

int xb_platform_disk_free(int drive, int64_t *bytes, int32_t *err) {
  if (drive < 0 || drive > 26) {
    *err = 15;
    return 0;
  }
  wchar_t root[4] = {(wchar_t)(L'A' + drive - 1), L':', L'\\', 0};
  ULARGE_INTEGER avail;
  if (!GetDiskFreeSpaceExW(drive ? root : NULL, &avail, NULL, NULL)) {
    *err = drive ? 15 : (int32_t)GetLastError();
    return 0;
  }
  *bytes = (int64_t)avail.QuadPart;
  return 1;
}

char *xb_platform_getenv(const char *name) {
  int32_t err;
  wchar_t *w = wide_path(name, &err);
  if (!w) return NULL;
  DWORD n = GetEnvironmentVariableW(w, NULL, 0);
  wchar_t *value = n ? malloc(n * sizeof *value) : NULL;
  char *out = NULL;
  if (value && GetEnvironmentVariableW(w, value, n) < n) out = utf8_of(value);
  free(value);
  free(w);
  return out;
}
