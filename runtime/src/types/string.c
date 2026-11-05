#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

xb_str *xb_str_alloc(size_t len) {
  if (len > SIZE_MAX / 2) xb_gc_fatal("out of memory"); /* the header must not wrap the size */
  xb_str *s = xb_gc_alloc(XB_GC_STRING, offsetof(xb_str, data) + len + 1);
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
