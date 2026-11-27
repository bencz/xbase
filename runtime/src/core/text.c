#include "xbrt_int.h"

#include <stdlib.h>
#include <string.h>

void xb_buffer_put(xb_buffer *b, const char *p, size_t n) {
  if (b->cap - b->len < n) {
    size_t cap = b->cap ? b->cap : 64;
    while (cap - b->len < n) cap *= 2;
    char *grown = b->heap ? realloc(b->p, cap) : malloc(cap);
    if (!grown) xb_gc_fatal("out of memory");
    if (!b->heap && b->len) memcpy(grown, b->p, b->len);
    b->p = grown;
    b->cap = cap;
    b->heap = 1;
  }
  if (n) memcpy(b->p + b->len, p, n);
  b->len += n;
}

void xb_buffer_free(xb_buffer *b) {
  if (b->heap) free(b->p);
}

int xb_name_is(const char *a, size_t len, const char *upper) {
  for (size_t i = 0; i < len; ++i)
    if (xb_ascii_upper(a[i]) != upper[i] || !upper[i]) return 0;
  return upper[len] == '\0';
}
