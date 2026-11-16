/* Byte-level text rules shared by the runtime (the ASCII letter case rule,
 * xb_ascii_upper and xb_ascii_lower, is in xbrt.h). */
#ifndef XB_TEXT_H
#define XB_TEXT_H

#include <stddef.h>

/* The blanks that leading trimming, Empty() and Val() skip. */
static inline int xb_is_blank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

/* A growing run of bytes (core/text.c): `p` is the caller's storage of `cap` bytes until it grows onto the heap
 * (`heap`); {0} starts one empty. xb_buffer_free frees what it allocated. */
typedef struct xb_buffer {
  char *p;
  size_t len, cap;
  int heap;
} xb_buffer;

void xb_buffer_put(xb_buffer *b, const char *p, size_t n);
void xb_buffer_free(xb_buffer *b);

#endif
