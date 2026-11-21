/* strings.prg in C: the same operations on the same strings, as a C program would write them (bytes in place, results
 * into buffers it reuses, memcpy, a growing buffer for appends), each N times; prints `<name> <ns per operation>`. The
 * sink keeps every result alive. */
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define N 4000000

static uint64_t sink;
static struct timespec t0;

static void start(void) { timespec_get(&t0, TIME_UTC); }

static void report(const char *name, double scale) {
  struct timespec t1;
  timespec_get(&t1, TIME_UTC);
  double ns = ((double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec)) * scale / N;
  printf("%-18s %8.1f\n", name, ns);
}

#define MEASURE(name, ...)                                                                                             \
  do {                                                                                                                 \
    start();                                                                                                           \
    for (long i = 1; i <= N; ++i) {                                                                                    \
      __VA_ARGS__;                                                                                                     \
    }                                                                                                                  \
    report(name, 1.0);                                                                                                 \
  } while (0)

static const char *S[4] = {"The quick brown fox jumps over the lazy dog 0123456789",
                           "Pack my box with five dozen liquor jugs, then go home!!",
                           "Sphinx of black quartz, judge my vow: 42 times, again.",
                           "How vexingly quick daft zebras jump over 9876543210 hay"};
static const char *P[4] = {"   padded words here   ", "  two  ", "none", "      six      "};

static size_t trim_left(const char *s, size_t n) {
  size_t a = 0;
  while (a < n && s[a] == ' ') ++a;
  return a;
}
static size_t trim_right(const char *s, size_t n) {
  while (n && s[n - 1] == ' ') --n;
  return n;
}

int main(void) {
  char buf[4096];
  static char longs[2100];
  for (int k = 0; k < 100; ++k) memcpy(longs + 10 * k, "abcdefghij", 10);
  memcpy(longs + 1000, "needle", 6);
  for (int k = 0; k < 100; ++k) memcpy(longs + 1006 + 10 * k, "klmnopqrst", 10);
  longs[2006] = 0;
  size_t slen[4], plen[4];
  for (int k = 0; k < 4; ++k) slen[k] = strlen(S[k]), plen[k] = strlen(P[k]);

  MEASURE("baseline", sink += (uintptr_t)S[i % 4]);
  MEASURE("len", sink += slen[i % 4]);
  MEASURE("asc", sink += (unsigned char)S[i % 4][0]);
  MEASURE("chr", buf[0] = (char)(i % 256); sink += (unsigned char)buf[0]);
  MEASURE("substr_1", sink += (unsigned char)S[i % 4][i % 50]);
  MEASURE("substr_1_dec", sink += (unsigned char)S[i % 4][i / 7 % 50]);
  MEASURE("asc_substr", sink += (unsigned char)S[i % 4][i % 50]);
  MEASURE("substr_10", memcpy(buf, S[i % 4] + i % 40, 10); sink += (unsigned char)buf[9]);
  MEASURE("left_10", memcpy(buf, S[i % 4], 10); sink += (unsigned char)buf[9]);
  MEASURE("right_10", memcpy(buf, S[i % 4] + slen[i % 4] - 10, 10); sink += (unsigned char)buf[9]);
  MEASURE("at_char", { const char *p = memchr(S[i % 4], 'z', slen[i % 4]); sink += p ? (uint64_t)(p - S[i % 4]) : 0; });
  MEASURE("at_word", { const char *p = strstr(S[i % 4], "over"); sink += p ? (uint64_t)(p - S[i % 4]) : 0; });
  MEASURE("at_long", { const char *p = strstr(longs, "needle"); sink += p ? (uint64_t)(p - longs) : 0; });
  MEASURE("rat_char", {
    size_t k = slen[i % 4];
    while (k && S[i % 4][k - 1] != 'o') --k;
    sink += k;
  });
  MEASURE("contains", sink += strstr(S[i % 4], "jump") != NULL);
  MEASURE("upper", {
    const char *s = S[i % 4];
    for (size_t k = 0; k < slen[i % 4]; ++k) buf[k] = (char)toupper((unsigned char)s[k]);
    sink += (unsigned char)buf[0];
  });
  MEASURE("lower", {
    const char *s = S[i % 4];
    for (size_t k = 0; k < slen[i % 4]; ++k) buf[k] = (char)tolower((unsigned char)s[k]);
    sink += (unsigned char)buf[0];
  });
  MEASURE("alltrim", {
    const char *s = P[i % 4];
    size_t a = trim_left(s, plen[i % 4]), b = trim_right(s, plen[i % 4]);
    memcpy(buf, s + a, b > a ? b - a : 0);
    sink += b > a ? b - a : 0;
  });
  MEASURE("ltrim", {
    size_t a = trim_left(P[i % 4], plen[i % 4]);
    memcpy(buf, P[i % 4] + a, plen[i % 4] - a);
    sink += a;
  });
  MEASURE("rtrim", { size_t b = trim_right(P[i % 4], plen[i % 4]); memcpy(buf, P[i % 4], b); sink += b; });
  MEASURE("padl_20", {
    size_t n = plen[i % 4];
    memset(buf, ' ', 30 - n);
    memcpy(buf + 30 - n, P[i % 4], n);
    sink += (unsigned char)buf[29];
  });
  MEASURE("padr_20", {
    size_t n = plen[i % 4];
    memcpy(buf, P[i % 4], n);
    memset(buf + n, ' ', 30 - n);
    sink += (unsigned char)buf[29];
  });
  MEASURE("padc_20", {
    size_t n = plen[i % 4], l = (30 - n) / 2;
    memset(buf, ' ', l);
    memcpy(buf + l, P[i % 4], n);
    memset(buf + l + n, ' ', 30 - n - l);
    sink += (unsigned char)buf[29];
  });
  MEASURE("replicate_20", {
    size_t n = plen[i % 4];
    for (int k = 0; k < 3; ++k) memcpy(buf + k * n, P[i % 4], n);
    sink += (unsigned char)buf[0];
  });
  MEASURE("space_20", memset(buf, ' ', (size_t)(i % 4 + 20)); sink += (unsigned char)buf[19]);
  MEASURE("strtran", {
    const char *s = S[i % 4];
    for (size_t k = 0; k < slen[i % 4]; ++k) buf[k] = s[k] == 'o' ? '0' : s[k];
    sink += (unsigned char)buf[1];
  });
  MEASURE("stuff", {
    const char *s = S[i % 4];
    memcpy(buf, s, 4);
    memcpy(buf + 4, "slow", 4);
    memcpy(buf + 8, s + 9, slen[i % 4] - 9);
    sink += (unsigned char)buf[8];
  });
  MEASURE("concat_2", {
    size_t a = slen[i % 4], b = plen[i % 4];
    memcpy(buf, S[i % 4], a);
    memcpy(buf + a, P[i % 4], b);
    sink += a + b;
  });
  MEASURE("concat_4_chars", {
    buf[0] = (char)(i % 26 + 65), buf[1] = (char)(i % 26 + 66), buf[2] = (char)(i % 26 + 67);
    buf[3] = (char)(i % 26 + 68);
    sink += (unsigned char)buf[3];
  });
  MEASURE("eq_exact", sink += slen[i % 4] == slen[(i + 1) % 4] && !memcmp(S[i % 4], S[(i + 1) % 4], slen[i % 4]));
  MEASURE("eq", sink += !strncmp(S[i % 4], "The quick", 9));
  MEASURE("lt", sink += strcmp(S[i % 4], S[(i + 1) % 4]) < 0);
  MEASURE("str_int", sink += (uint64_t)snprintf(buf, sizeof buf, "%10ld", i));
  MEASURE("str_dec", sink += (uint64_t)snprintf(buf, sizeof buf, "%12.4f", (double)i / 3));
  MEASURE("val", sink += (uint64_t)strtod(S[i % 4], NULL));
  MEASURE("transform", {
    long v = i;
    char t[16];
    int n = 0, g = 0;
    do {
      if (g == 3) t[n++] = ',', g = 0;
      t[n++] = (char)('0' + v % 10), v /= 10, ++g;
    } while (v);
    for (int k = 0; k < n; ++k) buf[k] = t[n - 1 - k];
    sink += (uint64_t)n;
  });
  MEASURE("isdigit", sink += isdigit((unsigned char)S[i % 4][i % 50]) != 0);
  MEASURE("empty", {
    const char *s = P[i % 4];
    size_t k = 0;
    while (k < plen[i % 4] && s[k] == ' ') ++k;
    sink += k == plen[i % 4];
  });
  {
    size_t cap = 16, len = 0;
    char *s = malloc(cap);
    start();
    for (long i = 1; i <= N; ++i) {
      if (len + 1 > cap) s = realloc(s, cap *= 2);
      s[len++] = (char)(i % 26 + 65);
    }
    report("append_1", 1.0);
    len = 0;
    start();
    for (long i = 1; i <= N; ++i) {
      if (len + 4 > cap) s = realloc(s, cap *= 2);
      memcpy(s + len, P[2], 4);
      len += 4;
    }
    report("append_4", 1.0);
    len = 0;
    start();
    for (long i = 1; i <= N; ++i) {
      if (len + 4 > cap) s = realloc(s, cap *= 2);
      s[len] = (char)(i % 26 + 65), s[len + 1] = (char)(i % 26 + 66), s[len + 2] = (char)(i % 26 + 67);
      s[len + 3] = (char)(i % 26 + 68);
      len += 4;
    }
    report("append_concat_4", 1.0);
    sink += len;
    free(s);
  }
  {
    size_t n = slen[0] * 1000;
    char *s = malloc(n);
    for (int k = 0; k < 1000; ++k) memcpy(s + k * slen[0], S[0], slen[0]);
    long passes = N / (long)n * 4;
    uint64_t acc = 0;
    start();
    for (long k = 0; k < passes; ++k)
      for (size_t i = 0; i < n; ++i) acc += (unsigned char)s[i];
    report("iterate_asc", 0.25);
    start();
    for (long k = 0; k < passes; ++k)
      for (size_t i = 0; i < n; ++i) acc += (unsigned char)s[i];
    report("iterate_foreach", 0.25);
    sink += acc;
    free(s);
  }
  printf("sink %llu\n", (unsigned long long)sink);
  return 0;
}
