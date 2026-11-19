/* bench/tokenize_split.prg in C: 200,000 comma-separated lines of 6 fields cut with strchr() into arrays of new
 * strings (the rest of the line taken into a new one after each field, as SubStr() does), the numeric fields read
 * with strtod() as Val() reads them; 2 rounds. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char **items;
  int64_t len, cap;
} Array;

static int64_t seed = 64;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static void aadd(Array *a, const char *s, size_t n) {
  if (a->len == a->cap) {
    a->cap = a->cap ? a->cap * 2 : 4;
    a->items = realloc(a->items, (size_t)a->cap * sizeof *a->items);
  }
  char *x = malloc(n + 1);
  memcpy(x, s, n);
  x[n] = '\0';
  a->items[a->len++] = x;
}

static char *copy(const char *s) {
  size_t n = strlen(s) + 1;
  return memcpy(malloc(n), s, n);
}

static Array split(const char *line, char sep) {
  Array a = {0};
  char *c = copy(line), *p;
  while ((p = strchr(c, sep)) != NULL) {
    aadd(&a, c, (size_t)(p - c));
    char *rest = copy(p + 1);
    free(c);
    c = rest;
  }
  aadd(&a, c, strlen(c));
  free(c);
  return a;
}

int main(void) {
  int64_t len = 200000;
  double n = 0;
  char **lines = malloc((size_t)len * sizeof *lines), z[16];
  for (int64_t i = 1; i <= len; ++i) {
    int64_t a = rnd(100000), b = rnd(1000), nz = rnd(12);
    memset(z, 'z', (size_t)nz);
    z[nz] = '\0';
    lines[i - 1] = malloc(64);
    /* Rand()'s % gives 2 decimals, which Str() shows */
    snprintf(lines[i - 1], 64, "%lld.00,name%lld,%lld.00,,%s,end", (long long)a, (long long)i, (long long)b, z);
  }
  long long t = bench_ms();
  for (int r = 1; r <= 2; ++r)
    for (int64_t i = 0; i < len; ++i) {
      Array a = split(lines[i], ',');
      n += strtod(a.items[0], NULL) + strtod(a.items[2], NULL) + (double)a.len + (double)strlen(a.items[4]);
      for (int64_t k = 0; k < a.len; ++k) free(a.items[k]);
      free(a.items);
    }
  bench_report("tokenize_split", t, n == 20190509958.0);
  for (int64_t i = 0; i < len; ++i) free(lines[i]);
  free(lines);
  return 0;
}
