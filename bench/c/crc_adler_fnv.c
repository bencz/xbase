/* bench/crc_adler_fnv.prg in C: twenty buffers of 1,000,000 bytes from an LCG, each hashed with CRC-32 (from a table),
 * Adler-32 and FNV-1a (32 bits); the functions are first checked against their published values. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

static int64_t table[256];

static int64_t crc32_of(const unsigned char *s, int64_t n) {
  int64_t crc = 0xFFFFFFFF;
  for (int64_t i = 0; i < n; ++i) crc = table[(crc ^ s[i]) & 0xFF] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFF;
}

static int64_t adler32_of(const unsigned char *s, int64_t n) {
  int64_t a = 1, b = 0;
  for (int64_t i = 0; i < n; ++i) {
    a += s[i];
    if (a >= 65521) a -= 65521;
    b += a;
    if (b >= 65521) b -= 65521;
  }
  return (b << 16) | a;
}

static int64_t fnv1a_of(const unsigned char *s, int64_t n) {
  int64_t h = 2166136261;
  for (int64_t i = 0; i < n; ++i) h = ((h ^ s[i]) * 16777619) & 0xFFFFFFFF;
  return h;
}

int main(void) {
  long long t = bench_ms();
  int64_t n = 1000000, seed = 42, total = 0;
  unsigned char *data = malloc((size_t)n);
  const unsigned char *digits = (const unsigned char *)"123456789", *wiki = (const unsigned char *)"Wikipedia";
  for (int64_t i = 0; i < 256; ++i) {
    int64_t c = i;
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
    table[i] = c;
  }
  int ok = crc32_of(digits, 9) == 0xCBF43926 && adler32_of(wiki, 9) == 0x11E60398 &&
           fnv1a_of((const unsigned char *)"a", 1) == 0xE40C292C;
  for (int rep = 1; rep <= 20; ++rep) {
    for (int64_t i = 0; i < n; ++i) {
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      data[i] = (unsigned char)(seed >> 16);
    }
    total += crc32_of(data, n) + adler32_of(data, n) + fnv1a_of(data, n);
  }
  free(data);
  bench_report("crc_adler_fnv", t, ok && total == 142519858956);
  return 0;
}
