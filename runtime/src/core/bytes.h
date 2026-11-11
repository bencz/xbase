/* Fields of the file formats, written and read in their own byte order
 * whatever the host's. */
#ifndef XB_BYTES_H
#define XB_BYTES_H

#include <stdint.h>
#include <string.h>

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
