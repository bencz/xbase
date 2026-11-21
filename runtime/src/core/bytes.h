/* Copies of bytes, and fields of the file formats, written and read in their own byte order whatever the host's. */
#ifndef XB_BYTES_H
#define XB_BYTES_H

#include <stdint.h>
#include <string.h>

/* `n` bytes from `src` to `dst`, which do not overlap: up to 16 as two copies of 8, 4, 2 or 1 bytes overlapping in
 * the middle (the first and the last of each size), which compilers make plain loads and stores, with no call; more
 * through memcpy, which then pays for itself (the compiled code's copyBytes does the same). */
static inline void xb_copy(void *dst, const void *src, size_t n) {
  char *d = dst;
  const char *s = src;
  if (n > 16) {
    memcpy(d, s, n);
  } else if (n >= 8) {
    uint64_t a, b;
    memcpy(&a, s, 8);
    memcpy(&b, s + n - 8, 8);
    memcpy(d, &a, 8);
    memcpy(d + n - 8, &b, 8);
  } else if (n >= 4) {
    uint32_t a, b;
    memcpy(&a, s, 4);
    memcpy(&b, s + n - 4, 4);
    memcpy(d, &a, 4);
    memcpy(d + n - 4, &b, 4);
  } else if (n >= 2) {
    uint16_t a, b;
    memcpy(&a, s, 2);
    memcpy(&b, s + n - 2, 2);
    memcpy(d, &a, 2);
    memcpy(d + n - 2, &b, 2);
  } else if (n) {
    *d = *s;
  }
}

static inline void xb_put_le16(unsigned char *p, uint16_t v) {
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
}
static inline uint16_t xb_get_le16(const unsigned char *p) { return (uint16_t)(p[0] | p[1] << 8); }

static inline void xb_put_le32(unsigned char *p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = (unsigned char)(v >> 8 * i);
}
static inline uint32_t xb_get_le32(const unsigned char *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline void xb_put_be32(unsigned char *p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = (unsigned char)(v >> 8 * (3 - i));
}
static inline uint32_t xb_get_be32(const unsigned char *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

static inline void xb_put_le64(unsigned char *p, uint64_t v) {
  for (int i = 0; i < 8; ++i) p[i] = (unsigned char)(v >> 8 * i);
}
static inline uint64_t xb_get_le64(const unsigned char *p) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= (uint64_t)p[i] << 8 * i;
  return v;
}

/* An IEEE double as its 8 bytes, little-endian. */
static inline void xb_put_le_double(unsigned char *p, double d) {
  uint64_t bits;
  memcpy(&bits, &d, 8);
  xb_put_le64(p, bits);
}
static inline double xb_get_le_double(const unsigned char *p) {
  uint64_t bits = xb_get_le64(p);
  double d;
  memcpy(&d, &bits, 8);
  return d;
}

#endif
