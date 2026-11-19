/* bench/url_encode.prg in C: 20,000 strings of 20 to 80 bytes (letters, digits, blanks, punctuation and bytes above
 * 127) percent-encoded as in a URL query (unreserved bytes kept, a blank as +, any other byte as %XX) into a new
 * buffer and decoded back into another; 8 rounds. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

#define HEX_DIGITS "0123456789ABCDEF"

static int64_t seed = 80;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static char *url_encode(const char *c) {
  char *out = malloc(strlen(c) * 3 + 1), *o = out;
  for (; *c; ++c) {
    unsigned char x = (unsigned char)*c;
    if (strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~", x))
      *o++ = (char)x;
    else if (x == ' ')
      *o++ = '+';
    else {
      *o++ = '%';
      *o++ = HEX_DIGITS[x / 16];
      *o++ = HEX_DIGITS[x % 16];
    }
  }
  *o = '\0';
  return out;
}

static char *url_decode(const char *c) {
  char *out = malloc(strlen(c) + 1), *o = out;
  while (*c) {
    if (*c == '%') {
      *o++ = (char)((strchr(HEX_DIGITS, c[1]) - HEX_DIGITS) * 16 + (strchr(HEX_DIGITS, c[2]) - HEX_DIGITS));
      c += 3;
    } else {
      *o++ = *c == '+' ? ' ' : *c;
      ++c;
    }
  }
  *o = '\0';
  return out;
}

int main(void) {
  const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJ0123456789     -_.~&=/?%:#+\xE9\xFC";
  int64_t nchars = (int64_t)strlen(chars), n = 0;
  static char a[20000][84];
  int ok = 1;
  for (int i = 0; i < 20000; ++i) {
    size_t k = 0;
    for (int64_t j = 1; j <= 20 + rnd(61); ++j) a[i][k++] = chars[rnd(nchars)];
    a[i][k] = '\0';
  }
  long long t = bench_ms();
  for (int r = 1; r <= 8; ++r)
    for (int i = 0; i < 20000; ++i) {
      char *enc = url_encode(a[i]), *dec;
      size_t len = strlen(enc);
      n += (int64_t)len + (unsigned char)enc[len - 1];
      dec = url_decode(enc);
      ok = ok && strcmp(dec, a[i]) == 0;
      free(enc);
      free(dec);
    }
  char *e = url_encode("a b&c=\xE9");
  ok = ok && n == 18744664 && strcmp(e, "a+b%26c%3D%E9") == 0;
  bench_report("url_encode", t, ok);
  free(e);
  return 0;
}
