/* Byte-level text work of the runtime (the rules the compiler shares, such as letter case and blanks, are in
 * xbrt.h). */
#ifndef XB_TEXT_H
#define XB_TEXT_H

#include <stddef.h>

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
