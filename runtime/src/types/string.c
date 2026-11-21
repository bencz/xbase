#include "xbrt_int.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(offsetof(xb_str1, bytes) == offsetof(xb_str, bytes), "xb_str1 has the layout of xb_str (xbrt.h)");

static const xb_str1 empty_string = {XB_STR_STATIC, 0, 0, {0, 0}};

xb_str *xb_str_empty(void) { return (xb_str *)&empty_string; }

#define XB_BYTE1(c) {XB_STR_STATIC, 0, 1, {(char)(c), 0}}
#define XB_BYTE4(c) XB_BYTE1(c), XB_BYTE1((c) + 1), XB_BYTE1((c) + 2), XB_BYTE1((c) + 3)
#define XB_BYTE16(c) XB_BYTE4(c), XB_BYTE4((c) + 4), XB_BYTE4((c) + 8), XB_BYTE4((c) + 12)
#define XB_BYTE64(c) XB_BYTE16(c), XB_BYTE16((c) + 16), XB_BYTE16((c) + 32), XB_BYTE16((c) + 48)
const xb_str1 xb_str_bytes[256] = {XB_BYTE64(0), XB_BYTE64(64), XB_BYTE64(128), XB_BYTE64(192)};

static xb_str *xb_str_byte(unsigned char c) { return (xb_str *)&xb_str_bytes[c]; }

static xb_str *str_alloc(uint64_t newLen, int fallible) {
  if (newLen > XB_STR_MAX_LEN) {
    if (fallible) return NULL;
    xb_gc_fatal("out of memory");
  }
  size_t len = (size_t)newLen;
  xb_str *s = (fallible ? xb_gc_try_alloc : xb_gc_alloc)(XB_GC_STRING, offsetof(xb_str, bytes) + len + 1);
  if (!s) return NULL;
  s->flags = 0;
  s->hash = 0;
  s->len = len;
  xb_str_buffer(s)[len] = '\0';
  return s;
}

xb_str *xb_str_alloc(uint64_t len) { return str_alloc(len, 0); }

xb_str *xb_str_try_alloc(uint64_t len) { return str_alloc(len, 1); }

xb_str *xb_str_growing(uint64_t len, uint64_t room) {
  if (room > UINT32_MAX) room = UINT32_MAX; /* the room must fit `hash` */
  if (room > XB_STR_MAX_LEN) room = len;
  xb_str *s = xb_str_alloc(room > len ? room : len);
  s->len = (size_t)len;
  xb_str_buffer(s)[len] = '\0';
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
  xb_copy(xb_str_buffer(s), p, len);
  return s;
}

/* The bytes of a string shorter than XB_STR_VIEW_MIN, which is never a view: xb_str_data without its test. */
static const char *short_data(const xb_str *s) { return s->bytes; }

/* a + b, `len` bytes, shorter than XB_STR_VIEW_MIN (most `+`): a plain string, the shared one for up to one byte. */
static xb_str *concat_short(const xb_str *a, const xb_str *b, size_t len) {
  if (len <= 1) return xb_str_new(a->len ? short_data(a) : short_data(b), len);
  xb_str *s = xb_str_alloc(len);
  xb_copy(xb_str_buffer(s), short_data(a), a->len);
  xb_copy(xb_str_buffer(s) + a->len, short_data(b), b->len);
  return s;
}

/* A view's object: the header, then its buffer's address where a plain string's bytes are. */
#define XB_STR_VIEW_SIZE (offsetof(xb_str, bytes) + sizeof(xb_strbuf *))
_Static_assert(offsetof(xb_str, bytes) % _Alignof(xb_strbuf *) == 0, "a view's buffer address is aligned");

/* A new view of `len` bytes, of no buffer yet: the caller sets it before anything else allocates. */
static xb_str *view_new(size_t len) {
  xb_str *v = xb_gc_alloc(XB_GC_VIEW, XB_STR_VIEW_SIZE);
  v->flags = XB_STR_VIEW;
  v->hash = 0;
  v->len = len;
  xb_str_view_set_buffer(v, NULL);
  return v;
}

/* a + b, `len` bytes, as a view of a new buffer with room for as much again; NULL when no memory holds that buffer.
 * The view is made first and rooted while the buffer is allocated: a buffer only a C variable holds would not survive
 * a collection. */
static xb_str *view_with_room(xb_ctx *ctx, const xb_str *a, const xb_str *b, size_t len) {
  size_t cap = len <= (XB_STR_MAX_LEN - 1) / 2 ? 2 * len + 1 : len + 1;
  xb_item held;
  xb_set_str(&held, view_new(0));
  xb_gc_root_push(ctx, &held);
  xb_strbuf *buf = xb_gc_try_alloc(XB_GC_STRBUF, offsetof(xb_strbuf, bytes) + cap);
  xb_gc_root_pop(ctx, 1);
  if (!buf) return NULL;
  xb_str *v = held.v.s; /* where the collection left it */
  buf->cap = cap;
  buf->used = len;
  memcpy(buf->bytes, xb_str_data(a), a->len);
  memcpy(buf->bytes + a->len, xb_str_data(b), b->len);
  buf->bytes[len] = '\0';
  v->len = len;
  xb_str_view_set_buffer(v, buf);
  xb_gc_write(v); /* a full collection in the buffer's allocation made the view old */
  return v;
}

/* xb_str_add for a result of XB_STR_VIEW_MIN bytes or more (every append to a view). */
XB_NOINLINE static xb_str *add_long(xb_ctx *ctx, const xb_str *a, const xb_str *b) {
  size_t n = b->len;
  if (!n || !a->len) return (xb_str *)(n ? b : a); /* nothing to copy: strings are immutable */
  if (a->flags & XB_STR_VIEW) {
    xb_strbuf *buf = xb_str_view_buffer(a);
    size_t tip = a->len;
    /* b's bytes claimed at the tip; another thread that appends to the same view meanwhile copies instead */
    if (n < buf->cap - tip && xb_atomic_cassz(&buf->used, &tip, tip + n, XB_RELAXED)) {
      memcpy(buf->bytes + tip, xb_str_data(b), n);
      buf->bytes[tip + n] = '\0';
      xb_str *v = view_new(tip + n); /* `a`, an operand's (a root), keeps the buffer meanwhile */
      xb_str_view_set_buffer(v, xb_str_view_buffer(a));
      return v;
    }
  }
  uint64_t len = (uint64_t)a->len + n;
  if (len <= XB_STR_MAX_LEN) {
    xb_str *v = view_with_room(ctx, a, b, (size_t)len);
    if (v) return v;
  }
  return xb_str_concat(a, b);
}

const char *xb_item_cstr(xb_item *it) {
  if (!(it->v.s->flags & XB_STR_VIEW)) return xb_str_data(it->v.s);
  xb_str *copy = xb_str_alloc(it->v.s->len);
  const xb_str *s = it->v.s; /* where the allocation's collection left it */
  memcpy(xb_str_buffer(copy), xb_str_data(s), s->len);
  xb_put(it, xb_header_word(XB_T_STRING, it->width, 0), xb_pointer_word(copy));
  return xb_str_data(copy);
}

xb_str *xb_str_concat(const xb_str *a, const xb_str *b) {
  size_t len = a->len + b->len;
  if (len < XB_STR_VIEW_MIN) return concat_short(a, b, len);
  xb_str *s = xb_str_alloc(len);
  xb_copy(xb_str_buffer(s), xb_str_data(a), a->len);
  xb_copy(xb_str_buffer(s) + a->len, xb_str_data(b), b->len);
  return s;
}

xb_str *xb_str_add(xb_ctx *ctx, const xb_str *a, const xb_str *b) {
  size_t len = a->len + b->len; /* two lengths of strings never wrap (XB_STR_MAX_LEN) */
  return len < XB_STR_VIEW_MIN ? concat_short(a, b, len) : add_long(ctx, a, b);
}

uint32_t xb_str_hash(xb_str *s) {
  int growing = (s->flags & XB_STR_APPEND) != 0;
  /* Threads may compute it at once: each stores the same value, atomically. */
  uint32_t known = growing ? 0 : xb_atomic_load32(&s->hash, XB_RELAXED);
  if (known) return known;
  uint32_t v = xb_hash_bytes(xb_str_data(s), s->len);
  if (!growing && !(s->flags & XB_STR_STATIC)) xb_atomic_store32(&s->hash, v, XB_RELAXED); /* strings are immutable */
  return v;
}
