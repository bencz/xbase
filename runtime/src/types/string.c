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

/* The most room a string can have and still be known: its object's size in
 * the header must stay below the saturated UINT32_MAX (xb_str_room). */
#define XB_STR_ROOM_MAX ((uint64_t)UINT32_MAX - 1 - offsetof(xb_str, data) - 1)

xb_str *xb_str_alloc_room(uint64_t len, uint64_t room) {
  if (room > XB_STR_ROOM_MAX || room > XB_STR_MAX_LEN) room = len; /* room that cannot be had or seen: exact size */
  xb_str *s = xb_str_alloc(room > len ? room : len);
  s->len = (size_t)len;
  s->data[len] = '\0';
  return s;
}

size_t xb_str_room(const xb_str *s) {
  uint32_t size = xb_gc_header(s)->size; /* saturated at UINT32_MAX: no room known */
  size_t header = offsetof(xb_str, data) + 1;
  return size == UINT32_MAX || size < header ? s->len : size - header;
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
