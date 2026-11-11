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

char *xb_file_path_with_ext(const char *name, size_t len, const char *ext) {
  size_t start = len;
  while (start > 0 && !xb_platform_path_separator(name[start - 1])) --start;
  size_t extLen = memchr(name + start, '.', len - start) ? 0 : strlen(ext);
  char *path = malloc(len + extLen + 1);
  if (!path) xb_gc_fatal("out of memory");
  memcpy(path, name, len);
  memcpy(path + len, ext, extLen);
  path[len + extLen] = '\0';
  return path;
}
