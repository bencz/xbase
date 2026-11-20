/* Random bytes for keys only the process knows (the keyed hashes of types/hash.c): the system's source, else the
 * process's own unpredictable values stirred by a linear congruential step (Knuth's MMIX constants). */
#include "platform/platform.h"

#include <stdint.h>

void xb_platform_random(void *buf, size_t n) {
  unsigned char *out = buf;
  size_t got = xb_platform_entropy(buf, n);
  uint64_t mix = (uint64_t)xb_platform_monotonic_ns() ^ (uint64_t)(uintptr_t)&mix ^ (uint64_t)(uintptr_t)buf << 7;
  for (; got < n; ++got) {
    mix = mix * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
    out[got] = (unsigned char)(mix >> 56);
  }
}
