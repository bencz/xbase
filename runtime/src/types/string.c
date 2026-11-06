#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(offsetof(xb_str1, data) == offsetof(xb_str, data), "xb_str1 has the layout of xb_str (xbrt.h)");

static const xb_str1 empty_string = {XB_STR_STATIC, 0, 0, {0, 0}};

xb_str *xb_str_empty(void) { return (xb_str *)&empty_string; }

#define XB_BYTE1(c) {XB_STR_STATIC, 0, 1, {(char)(c), 0}}
#define XB_BYTE4(c) XB_BYTE1(c), XB_BYTE1((c) + 1), XB_BYTE1((c) + 2), XB_BYTE1((c) + 3)
#define XB_BYTE16(c) XB_BYTE4(c), XB_BYTE4((c) + 4), XB_BYTE4((c) + 8), XB_BYTE4((c) + 12)
#define XB_BYTE64(c) XB_BYTE16(c), XB_BYTE16((c) + 16), XB_BYTE16((c) + 32), XB_BYTE16((c) + 48)
const xb_str1 xb_str_bytes[256] = {XB_BYTE64(0), XB_BYTE64(64), XB_BYTE64(128), XB_BYTE64(192)};

static xb_str *xb_str_byte(unsigned char c) { return (xb_str *)&xb_str_bytes[c]; }

xb_str *xb_str_alloc(uint64_t newLen) {
  if (newLen > XB_STR_MAX_LEN) xb_gc_fatal("out of memory");
  size_t len = (size_t)newLen;
  xb_str *s = xb_gc_alloc(XB_GC_STRING, offsetof(xb_str, data) + len + 1);
  s->flags = 0;
  s->hash = 0;
  s->len = len;
  s->data[len] = '\0';
  return s;
}

xb_str *xb_str_growing(uint64_t len, uint64_t room) {
  if (room > UINT32_MAX) room = UINT32_MAX; /* the room must fit `hash` */
  if (room > XB_STR_MAX_LEN) room = len;
  xb_str *s = xb_str_alloc(room > len ? room : len);
  s->len = (size_t)len;
  s->data[len] = '\0';
  if (room > len) { /* without room to grow (a string past 4 GB), an ordinary string */
    s->flags = XB_STR_APPEND;
    s->hash = (uint32_t)room;
  }
  return s;
}

void xb_str_share(xb_str *s) {
  if (!(s->flags & XB_STR_APPEND)) return;
  s->flags &= ~XB_STR_APPEND;
  s->hash = 0; /* not computed */
}

xb_str *xb_str_new(const char *p, size_t len) {
  if (len <= 1) return len ? xb_str_byte((unsigned char)p[0]) : xb_str_empty();
  xb_str *s = xb_str_alloc(len);
  if (len) memcpy(s->data, p, len);
  return s;
}

xb_str *xb_str_concat(const xb_str *a, const xb_str *b) {
  if (a->len + b->len <= 1) return xb_str_new(a->len ? a->data : b->data, a->len + b->len);
  xb_str *s = xb_str_alloc(a->len + b->len);
  memcpy(s->data, a->data, a->len);
  memcpy(s->data + a->len, b->data, b->len);
  return s;
}
