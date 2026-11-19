/* bench/at_rat_strtran.prg in C: the first and last position of 200 words in a 20,000-byte text of those words, and
 * each word replaced by a longer one in a new string (allocated and freed, as StrTran() makes one); 80 rounds. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

static int64_t seed = 33;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static int64_t at(const char *w, const char *c) {
  const char *p = strstr(c, w);
  return p ? p - c + 1 : 0;
}

static int64_t rat(const char *w, const char *c, size_t clen) {
  size_t wlen = strlen(w);
  if (wlen > clen) return 0;
  for (size_t i = clen - wlen + 1; i-- > 0;)
    if (c[i] == w[0] && memcmp(c + i, w, wlen) == 0) return (int64_t)i + 1;
  return 0;
}

/* c with every w replaced by r, in a new string; its length in *len. noinline and noipa: the string is made though
 * the program reads only its length. */
__attribute__((noinline, noipa)) static char *strtran(const char *c, size_t clen, const char *w, const char *r,
                                                      size_t *len) {
  size_t wlen = strlen(w), rlen = strlen(r), count = 0, n = 0;
  for (const char *p = strstr(c, w); p; p = strstr(p + wlen, w)) ++count;
  char *out = malloc(clen + count * rlen - count * wlen + 1);
  const char *from = c;
  for (const char *p = strstr(c, w); p; p = strstr(p + wlen, w)) {
    memcpy(out + n, from, (size_t)(p - from));
    n += (size_t)(p - from);
    memcpy(out + n, r, rlen);
    n += rlen;
    from = p + wlen;
  }
  memcpy(out + n, from, clen - (size_t)(from - c));
  n += clen - (size_t)(from - c);
  out[n] = '\0';
  *len = n;
  return out;
}

int main(void) {
  char words[200][16], c[20100], r[24];
  size_t clen = 0;
  int64_t n = 0;
  for (int i = 0; i < 200; ++i) {
    int len = 0;
    for (int64_t j = 1; j <= 3 + rnd(6); ++j) words[i][len++] = (char)(97 + rnd(26));
    words[i][len] = '\0';
  }
  while (clen < 20000) {
    const char *w = words[rnd(rnd(200) + 1)];
    size_t wlen = strlen(w);
    memcpy(c + clen, w, wlen);
    clen += wlen;
    c[clen++] = ' ';
  }
  c[clen] = '\0';
  long long t = bench_ms();
  for (int round = 1; round <= 80; ++round)
    for (int i = 0; i < 200; ++i) {
      const char *w = words[i];
      size_t len, wlen = strlen(w);
      r[0] = '<';
      memcpy(r + 1, w, wlen);
      r[wlen + 1] = '>';
      r[wlen + 2] = '\0';
      char *s = strtran(c, clen, w, r, &len);
      n += at(w, c) + rat(w, c, clen) + (int64_t)len;
      free(s);
    }
  bench_report("at_rat_strtran", t, n == 620527280);
  return 0;
}
