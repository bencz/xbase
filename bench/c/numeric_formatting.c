/* bench/numeric_formatting.prg in C: 80,000 numbers with three decimals (-50,000 to 50,000) and integers (0 to
 * 999,999) from a fixed seed, each formatted at the .prg's widths and decimals: as Str (snprintf()), StrZero, Round,
 * and Transform with numeric pictures (thousands separators, @E, @( for negatives in parentheses, @Z) and @R on a
 * string (a CPF mask), written here for the pictures the program uses; read back with strtod() as Val() reads them.
 * The text is appended to one growing buffer. The text's length, its separators (counted from the length of a copy
 * without them, as the .prg does) and the values read back are checked. */
#include "bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *data;
  size_t len, cap;
} Buf;

static int64_t seed = 314159;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static void put(Buf *b, const char *s) {
  size_t n = strlen(s);
  if (b->len + n + 1 > b->cap) {
    while (b->len + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 64;
    b->data = realloc(b->data, b->cap);
  }
  memcpy(b->data + b->len, s, n + 1);
  b->len += n;
}

/* Str( x, width, dec ): a negative number that rounds to zero is shown without its sign. */
static void str(char *out, size_t size, double x, int width, int dec) {
  snprintf(out, size, "%*.*f", width, dec, x);
  const char *minus = strchr(out, '-');
  if (minus && strspn(minus + 1, "0.") == strlen(minus + 1)) snprintf(out, size, "%*.*f", width, dec, 0.0);
}

/* Transform( x, pic ) for a numeric picture of 9s, "," and "."; e: the @E function (separators swapped); paren: the
 * @( function (a negative number in parentheses, whose digits become * when it takes every position); zero: the @Z
 * function (a blank field for zero). */
static void transform(char *out, double x, const char *pic, int e, int paren, int zero) {
  int intd = 0, dec = 0, seen_dot = 0;
  size_t n = strlen(pic), k = 0;
  char digits[48];
  for (const char *p = pic; *p; ++p)
    if (*p == '.')
      seen_dot = 1;
    else if (*p == '9')
      ++*(seen_dot ? &dec : &intd);
  snprintf(digits, sizeof digits, "%*.*f", intd + (dec ? dec + 1 : 0), dec, fabs(x));
  int negative = x < 0 && strspn(digits, " 0.") != strlen(digits);
  if (zero && strspn(digits, " 0.") == strlen(digits)) {
    memset(out, ' ', n);
    out[n] = '\0';
    return;
  }
  for (size_t i = 0; i < n; ++i) {
    char c = pic[i];
    if (c == '9' || c == '.')
      out[i] = digits[k++];
    else if (c == ',')
      out[i] = i > 0 && out[i - 1] >= '0' && out[i - 1] <= '9' ? ',' : ' ';
    else
      out[i] = c;
  }
  out[n] = '\0';
  if (paren && x < 0) {
    if (out[0] != ' ')
      for (size_t i = 0; i < n; ++i)
        if (out[i] >= '0' && out[i] <= '9') out[i] = '*';
    out[0] = '(';
    out[n] = ')';
    out[n + 1] = '\0';
  } else if (negative) {
    size_t first = strspn(out, " ");
    if (first == 0) {
      for (size_t i = 0; i < n; ++i)
        if (out[i] != '.') out[i] = '*';
    } else
      out[first - 1] = '-';
  }
  if (e)
    for (char *p = out; *p; ++p) *p = *p == ',' ? '.' : *p == '.' ? ',' : *p;
}

/* Transform( c, "@R " + pic ): the 9s of pic take c's characters in turn, the others are inserted. */
static void transform_r(char *out, const char *c, const char *pic) {
  for (; *pic; ++pic) *out++ = *pic == '9' ? *c++ : *pic;
  *out = '\0';
}

/* StrTran( c, x ): a new string without the byte x; its length. noinline and noipa: the string is made though the
 * program reads only its length. */
__attribute__((noinline, noipa)) static size_t without(const char *c, size_t len, char x) {
  char *out = malloc(len + 1);
  size_t n = 0;
  for (size_t i = 0; i < len; ++i)
    if (c[i] != x) out[n++] = c[i];
  out[n] = '\0';
  free(out);
  return n;
}

static double round_dec(double x, int dec) {
  double p = pow(10, dec);
  return round(x * p) / p;
}

int main(void) {
  long long t = bench_ms();
  Buf c = {0};
  char a[40], b[40], d[40], e[40], f[40], g[40], h[40], z[40], line[320];
  int64_t cents = 0, tenths = 0, comma, dot, paren;
  for (int64_t i = 1; i <= 80000; ++i) {
    int64_t k = rnd(100000000) - 50000000;
    if (llabs(k) % 10 == 5) ++k;
    double x = (double)k / 1000;
    int64_t n = rnd(1000000);
    str(a, sizeof a, x, 14, 2);
    snprintf(b, sizeof b, "%08lld", (long long)n);
    transform(d, x, "999,999.99", 1, 0, 0);
    transform(e, x, "999,999.999", 0, 0, 0);
    snprintf(z, sizeof z, "%011lld", (long long)n);
    transform_r(f, z, "999.999.999-99");
    transform(g, x, "99,999.99", 0, 1, 0);
    transform(h, (double)(n % 3), "9", 0, 0, 1);
    str(z, sizeof z, round_dec(x, 1), 12, 1);
    snprintf(line, sizeof line, "%s%s%s%s%s%s%s%s\n", a, b, d, e, f, g, h, z + strspn(z, " "));
    put(&c, line);
    cents += (int64_t)round(strtod(a, NULL) * 100);
    tenths += (int64_t)round(round_dec(x, 1) * 10);
  }
  comma = (int64_t)(c.len - without(c.data, c.len, ','));
  dot = (int64_t)(c.len - without(c.data, c.len, '.'));
  paren = (int64_t)(c.len - without(c.data, c.len, '('));
  int ok = c.len == 6065015 && comma == 236856 && dot == 558428 && paren == 41318 && cents == -6496080269LL &&
           tenths == -649608444 &&
           strncmp(c.data, "      35103.0200005610 35.103,02 35,103.019000.000.056-1035,103.02 35103.0\n", 75) == 0;
  bench_report("numeric_formatting", t, ok);
  free(c.data);
  return 0;
}
