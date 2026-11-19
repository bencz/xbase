/* bench/md5_sha1.prg in C: MD5 and SHA-1 (32-bit words in 64-bit integers, masked) of a 200,000-byte buffer from an
 * LCG, 40 times, padded as the standards say; first checked against the published digests of "" and "abc". */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

#define MASK 0xFFFFFFFF

static const int64_t md5_k[64] = {
  0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
  0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
  0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
  0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
  0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
  0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
  0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
  0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391};
static const int md5_r[16] = {7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21};

static int64_t rotl(int64_t x, int s) { return ((x << s) | (x >> (32 - s))) & MASK; }

static void hex(char *out, const int64_t *w, int n, int little) {
  static const char digits[] = "0123456789abcdef";
  for (int i = 0; i < n; ++i)
    for (int b = 0; b < 4; ++b) {
      int64_t byte = (w[i] >> (little ? 8 * b : 24 - 8 * b)) & 0xFF;
      *out++ = digits[byte >> 4];
      *out++ = digits[byte & 15];
    }
  *out = 0;
}

/* The message, a 0x80 byte, zeros up to 56 bytes modulo 64, then its length in bits in 8 bytes. */
static unsigned char *pad(const unsigned char *s, int64_t n, int little, int64_t *padded) {
  int64_t len = (n + 8) / 64 * 64 + 64;
  unsigned char *p = calloc((size_t)len, 1);
  memcpy(p, s, (size_t)n);
  p[n] = 0x80;
  for (int i = 0; i < 8; ++i) p[len - 8 + (little ? i : 7 - i)] = (unsigned char)(((n * 8) >> (8 * i)) & 0xFF);
  *padded = len;
  return p;
}

static void md5(const unsigned char *s, int64_t n, char *out) {
  int64_t len, h[4] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476}, m[16];
  unsigned char *p = pad(s, n, 1, &len);
  for (int64_t blk = 0; blk < len; blk += 64) {
    for (int j = 0; j < 16; ++j)
      m[j] = p[blk + 4 * j] | (p[blk + 4 * j + 1] << 8) | (p[blk + 4 * j + 2] << 16) |
             ((int64_t)p[blk + 4 * j + 3] << 24);
    int64_t a = h[0], b = h[1], c = h[2], d = h[3], f, tmp;
    int g;
    for (int i = 0; i < 64; ++i) {
      if (i < 16) {
        f = (b & c) | ((b ^ MASK) & d);
        g = i;
      } else if (i < 32) {
        f = (d & b) | ((d ^ MASK) & c);
        g = (5 * i + 1) & 15;
      } else if (i < 48) {
        f = b ^ c ^ d;
        g = (3 * i + 5) & 15;
      } else {
        f = c ^ (b | (d ^ MASK));
        g = (7 * i) & 15;
      }
      tmp = d;
      d = c;
      c = b;
      b = (b + rotl((a + f + md5_k[i] + m[g]) & MASK, md5_r[(i >> 4) * 4 + (i & 3)])) & MASK;
      a = tmp;
    }
    h[0] = (h[0] + a) & MASK;
    h[1] = (h[1] + b) & MASK;
    h[2] = (h[2] + c) & MASK;
    h[3] = (h[3] + d) & MASK;
  }
  free(p);
  hex(out, h, 4, 1);
}

static void sha1(const unsigned char *s, int64_t n, char *out) {
  int64_t len, h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0}, w[80];
  unsigned char *p = pad(s, n, 0, &len);
  for (int64_t blk = 0; blk < len; blk += 64) {
    for (int j = 0; j < 16; ++j)
      w[j] = ((int64_t)p[blk + 4 * j] << 24) | (p[blk + 4 * j + 1] << 16) |
             (p[blk + 4 * j + 2] << 8) | p[blk + 4 * j + 3];
    for (int j = 16; j < 80; ++j) w[j] = rotl(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
    int64_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f, k, tmp;
    for (int i = 0; i < 80; ++i) {
      if (i < 20) {
        f = (b & c) | ((b ^ MASK) & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      tmp = (rotl(a, 5) + f + e + k + w[i]) & MASK;
      e = d;
      d = c;
      c = rotl(b, 30);
      b = a;
      a = tmp;
    }
    h[0] = (h[0] + a) & MASK;
    h[1] = (h[1] + b) & MASK;
    h[2] = (h[2] + c) & MASK;
    h[3] = (h[3] + d) & MASK;
    h[4] = (h[4] + e) & MASK;
  }
  free(p);
  hex(out, h, 5, 0);
}

int main(void) {
  long long t = bench_ms();
  int64_t n = 200000, seed = 7;
  unsigned char *data = malloc((size_t)n);
  char m5[33], s1[41];
  md5((const unsigned char *)"", 0, m5);
  int ok = strcmp(m5, "d41d8cd98f00b204e9800998ecf8427e") == 0;
  md5((const unsigned char *)"abc", 3, m5);
  ok = ok && strcmp(m5, "900150983cd24fb0d6963f7d28e17f72") == 0;
  sha1((const unsigned char *)"", 0, s1);
  ok = ok && strcmp(s1, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0;
  sha1((const unsigned char *)"abc", 3, s1);
  ok = ok && strcmp(s1, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0;
  for (int64_t i = 0; i < n; ++i) {
    seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
    data[i] = (unsigned char)((seed >> 16) & 0xFF);
  }
  for (int rep = 1; rep <= 40; ++rep) {
    md5(data, n, m5);
    sha1(data, n, s1);
    ok = ok && strcmp(m5, "9caadd7dbbe1174da261d335b92f85fb") == 0 &&
         strcmp(s1, "cec3d79455d923d234a1a40741be135a4ba980e2") == 0;
  }
  free(data);
  bench_report("md5_sha1", t, ok);
  return 0;
}
