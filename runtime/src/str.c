#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TODO(gc): strings are malloc'ed and never freed until the tracing GC
 * lands. Static (compiler-emitted) strings carry XB_STR_STATIC. */
xb_str *xb_str_alloc(size_t len) {
  xb_str *s = malloc(offsetof(xb_str, data) + len + 1);
  if (!s) {
    fputs("xbrt: out of memory (string)\n", stderr);
    abort();
  }
  s->flags = 0;
  s->hash = 0;
  s->len = len;
  s->data[len] = '\0';
  return s;
}

xb_str *xb_str_new(const char *p, size_t len) {
  xb_str *s = xb_str_alloc(len);
  if (len) memcpy(s->data, p, len);
  return s;
}

xb_str *xb_str_concat(const xb_str *a, const xb_str *b) {
  xb_str *s = xb_str_alloc(a->len + b->len);
  memcpy(s->data, a->data, a->len);
  memcpy(s->data + a->len, b->data, b->len);
  return s;
}
