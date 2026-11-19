/* bench/substr_massivo.prg in C: 6,000,000 pieces of 1 to 20 bytes copied out of a 1,000,000-byte text at spread
 * positions (by a function the compiler may neither inline nor fold, noinline and noipa, so that each copy is made as
 * SubStr() makes it), their first and last bytes read. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

static int64_t seed = 5;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

__attribute__((noinline, noipa)) static size_t substr(char *out, const char *c, size_t clen, int64_t start,
                                                      int64_t count) {
  size_t from = (size_t)(start - 1), n = from + (size_t)count <= clen ? (size_t)count : clen - from;
  memcpy(out, c + from, n);
  out[n] = '\0';
  return n;
}

int main(void) {
  size_t clen = 1000000;
  char *c = malloc(clen + 1), s[32];
  int64_t n = 0;
  for (size_t i = 0; i < clen; ++i) c[i] = (char)(33 + rnd(90));
  c[clen] = '\0';
  long long t = bench_ms();
  for (int64_t i = 1; i <= 6000000; ++i) {
    size_t len = substr(s, c, clen, i * 7919 % 999970 + 1, i % 20 + 1);
    n += (unsigned char)s[0] + (unsigned char)s[len - 1] + (int64_t)len;
  }
  bench_report("substr_massivo", t, n == 994529563);
  free(c);
  return 0;
}
