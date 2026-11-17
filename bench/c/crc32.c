/* bench/crc32.prg in C: CRC-32 of 20,000 bytes (the table made each time, the data built in chunks), 100 times. */
#include "bench.h"
#include <stdint.h>
#include <string.h>

static uint32_t crc32_of_20k(void) {
  uint32_t table[256];
  static unsigned char data[20000];
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    table[i] = c;
  }
  size_t len = 0;
  for (int i = 0; i < 20000; i += 100) {
    unsigned char chunk[100];
    for (int k = i; k < i + 100; ++k) chunk[k - i] = (unsigned char)((k * 31 + 7) % 251);
    memcpy(data + len, chunk, 100);
    len += 100;
  }
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFFu;
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 100; ++rep) ok = ok && crc32_of_20k() == 2954121016u;
  bench_report("crc32", t, ok);
  return 0;
}
