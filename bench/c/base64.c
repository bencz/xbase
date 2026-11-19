/* bench/base64.prg in C: 1,200,000 bytes from a fixed-seed generator encoded to Base64 (three bytes to four characters
 * through a table) and decoded back, each into a new buffer; 3 rounds. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int64_t seed = 64;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static char *encode(const unsigned char *c, size_t len, size_t *outlen) {
  size_t rest = len % 3, o = 0;
  char *out = malloc((len + 2) / 3 * 4 + 1);
  for (size_t i = 0; i < len - rest; i += 3) {
    int64_t n = c[i] * 65536 + c[i + 1] * 256 + c[i + 2];
    out[o++] = table[n / 262144];
    out[o++] = table[n / 4096 % 64];
    out[o++] = table[n / 64 % 64];
    out[o++] = table[n % 64];
  }
  if (rest == 1) {
    int64_t n = c[len - 1];
    out[o++] = table[n / 4];
    out[o++] = table[n % 4 * 16];
    out[o++] = '=';
    out[o++] = '=';
  } else if (rest == 2) {
    int64_t n = c[len - 2] * 256 + c[len - 1];
    out[o++] = table[n / 1024];
    out[o++] = table[n / 16 % 64];
    out[o++] = table[n % 16 * 4];
    out[o++] = '=';
  }
  out[o] = '\0';
  *outlen = o;
  return out;
}

static unsigned char *decode(const char *c, size_t len, size_t *outlen) {
  static int64_t value[256];
  static int ready;
  size_t pad = 0, o = 0;
  if (!ready) {
    for (int i = 0; i < 64; ++i) value[(unsigned char)table[i]] = i;
    ready = 1;
  }
  if (len >= 2 && c[len - 2] == '=' && c[len - 1] == '=')
    pad = 2;
  else if (len >= 1 && c[len - 1] == '=')
    pad = 1;
  unsigned char *out = malloc(len / 4 * 3 + 1);
  for (size_t i = 0; i < len; i += 4) {
    int64_t n = value[(unsigned char)c[i]] * 262144 + value[(unsigned char)c[i + 1]] * 4096 +
                value[(unsigned char)c[i + 2]] * 64 + value[(unsigned char)c[i + 3]];
    out[o++] = (unsigned char)(n / 65536);
    out[o++] = (unsigned char)(n / 256 % 256);
    out[o++] = (unsigned char)(n % 256);
  }
  *outlen = o - pad;
  return out;
}

int main(void) {
  size_t len = 1200000;
  unsigned char *c = malloc(len);
  int64_t n = 0;
  int ok = 1;
  for (size_t i = 0; i < len; ++i) c[i] = (unsigned char)rnd(256);
  long long t = bench_ms();
  for (size_t r = 1; r <= 3; ++r) {
    size_t elen, dlen, part = len - r + 1;
    char *enc = encode(c, part, &elen);
    unsigned char *dec = decode(enc, elen, &dlen);
    ok = ok && dlen == part && memcmp(dec, c, part) == 0;
    n += (int64_t)elen;
    for (size_t i = 0; i < elen; i += 97) n += (unsigned char)enc[i];
    free(enc);
    free(dec);
  }
  size_t l1, l2;
  char *ma = encode((const unsigned char *)"Ma", 2, &l1);
  unsigned char *m = decode("TQ==", 4, &l2);
  ok = ok && n == 9038319 && strcmp(ma, "TWE=") == 0 && l2 == 1 && m[0] == 'M';
  bench_report("base64", t, ok);
  free(ma);
  free(m);
  free(c);
  return 0;
}
