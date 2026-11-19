/* bench/upper_lower_alltrim.prg in C: upper case, lower case, and the blanks cut from both ends, the left and the
 * right of 1,000 mixed-case strings padded with blanks on both sides, 1,500,000 times; each result made in a buffer
 * of its own by a function the compiler may neither inline nor fold (noinline, noipa), as the runtime's functions make
 * a new string. */
#include "bench.h"
#include <ctype.h>
#include <string.h>

typedef struct {
  char s[48];
} Str;

static int64_t seed = 8;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

#define STRFN __attribute__((noinline, noipa)) static size_t

STRFN alltrim(char *out, const char *s) {
  size_t n = strlen(s);
  while (n && s[n - 1] == ' ') --n;
  while (n && *s == ' ') {
    ++s;
    --n;
  }
  memcpy(out, s, n);
  out[n] = '\0';
  return n;
}

STRFN ltrim(char *out, const char *s) {
  while (*s == ' ') ++s;
  size_t n = strlen(s);
  memcpy(out, s, n + 1);
  return n;
}

STRFN rtrim(char *out, const char *s) {
  size_t n = strlen(s);
  while (n && s[n - 1] == ' ') --n;
  memcpy(out, s, n);
  out[n] = '\0';
  return n;
}

STRFN upper(char *out, const char *s) {
  size_t n = 0;
  for (; s[n]; ++n) out[n] = (char)toupper((unsigned char)s[n]);
  out[n] = '\0';
  return n;
}

STRFN lower(char *out, const char *s) {
  size_t n = 0;
  for (; s[n]; ++n) out[n] = (char)tolower((unsigned char)s[n]);
  out[n] = '\0';
  return n;
}

int main(void) {
  static Str a[1000];
  const char *chars = "aBcDeFgHiJkLmNoPqRsTuVwXyZ 0123456789";
  char w[48], u[48], l[48], r[48], lt[48];
  int64_t n = 0;
  for (int i = 0; i < 1000; ++i) {
    size_t k = 0;
    for (int64_t j = 1; j <= 5 + rnd(30); ++j) w[k++] = chars[rnd(37)];
    w[k] = '\0';
    int64_t left = rnd(6);
    size_t m = (size_t)left;
    memset(a[i].s, ' ', m);
    a[i].s[m++] = 'x';
    memcpy(a[i].s + m, w, k);
    m += k;
    a[i].s[m++] = 'Q';
    int64_t right = rnd(6);
    memset(a[i].s + m, ' ', (size_t)right);
    a[i].s[m + (size_t)right] = '\0';
  }
  long long t = bench_ms();
  for (int64_t i = 1; i <= 1500000; ++i) {
    const char *x = a[i % 1000].s;
    size_t len = alltrim(w, x);
    upper(u, w);
    size_t llen = lower(l, w);
    n += (int64_t)len + (unsigned char)u[0] + (unsigned char)l[llen - 1] + (int64_t)rtrim(r, x) -
         (int64_t)ltrim(lt, x);
  }
  bench_report("upper_lower_alltrim", t, n == 320344500);
  return 0;
}
