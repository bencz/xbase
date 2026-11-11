/* File helpers shared by the runtime's file users (core/file.h). */
#include "core/file.h"

#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

int64_t xb_file_read_up_to(xb_file f, void *buf, size_t n, int32_t *err) {
  size_t got = 0;
  while (got < n) {
    int64_t r = xb_platform_file_read(f, (char *)buf + got, n - got, err);
    if (r < 0) return -1;
    if (r == 0) break;
    got += (size_t)r;
  }
  return (int64_t)got;
}

int xb_file_read_at(xb_file f, int64_t pos, void *buf, size_t n, int32_t *err) {
  if (xb_platform_file_seek(f, pos, FS_SET, err) != pos) return 0;
  int64_t got = xb_file_read_up_to(f, buf, n, err);
  if (got >= 0 && (size_t)got < n) *err = XB_FERR_FAILURE;
  return got >= 0 && (size_t)got == n;
}

int xb_file_write_at(xb_file f, int64_t pos, const void *buf, size_t n, int32_t *err) {
  return xb_platform_file_seek(f, pos, FS_SET, err) == pos && xb_platform_file_write(f, buf, n, err);
}

void xb_file_name_parts(const char *name, size_t len, size_t *start, size_t *ext) {
  *start = len;
  while (*start > 0 && !xb_platform_path_separator(name[*start - 1])) --*start;
  *ext = len;
  for (size_t i = len; i-- > *start;)
    if (name[i] == '.') {
      *ext = i;
      break;
    }
}

/* `len` bytes of `name`, then `extLen` bytes of `ext`: malloc'ed, NUL ended. */
static char *joined(const char *name, size_t len, const char *ext, size_t extLen) {
  char *path = malloc(len + extLen + 1);
  if (!path) xb_gc_fatal("out of memory");
  memcpy(path, name, len);
  memcpy(path + len, ext, extLen);
  path[len + extLen] = '\0';
  return path;
}

char *xb_file_path_replace_ext(const char *path, const char *ext) {
  size_t start, stem;
  xb_file_name_parts(path, strlen(path), &start, &stem);
  return joined(path, stem, ext, strlen(ext));
}

char *xb_file_path_with_ext(const char *name, size_t len, const char *ext) {
  size_t start, stem;
  xb_file_name_parts(name, len, &start, &stem);
  return joined(name, len, ext, stem < len ? 0 : strlen(ext));
}
