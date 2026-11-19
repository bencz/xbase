/* bench/long_short_mix.prg in C: 100,000 records that live to the end and 2,500,000 temporary records { number,
 * string, empty } with their strings, one in 8 of them stored into a long-lived record (an old value pointing to a new
 * one), one record in 64 steps replaced by a new one; each value is freed when the last reference to it goes. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Rec Rec;
struct Rec {
  int64_t n;
  char *s;
  Rec *x; /* NULL for NIL */
};

static int64_t seed = 17;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

/* Str( n ): right-aligned in 10 characters. */
static char *str10(int64_t n) {
  char *s = malloc(24);
  snprintf(s, 24, "%10lld", (long long)n);
  return s;
}

static Rec *rec(int64_t n, char *s, Rec *x) {
  Rec *r = malloc(sizeof *r);
  *r = (Rec){n, s, x};
  return r;
}

static void free_rec(Rec *r) {
  if (!r) return;
  free(r->s);
  free(r);
}

int main(void) {
  long long t = bench_ms();
  int64_t len = 100000, n = 0;
  Rec **lng = malloc((size_t)len * sizeof *lng);
  for (int64_t i = 1; i <= len; ++i) lng[i - 1] = rec(i, str10(i), NULL);
  for (int64_t i = 1; i <= 2500000; ++i) {
    size_t slen = (size_t)(i % 40 + 1);
    char *s = malloc(slen + 1);
    memset(s, 97 + i % 26, slen);
    s[slen] = '\0';
    Rec *x = rec(i % 1000, s, NULL);
    n += (int64_t)strlen(x->s);
    if (i % 8 == 0) {
      Rec *r = lng[rnd(len)];
      free_rec(r->x);
      r->x = x;
    } else
      free_rec(x);
    if (i % 64 == 0) {
      int64_t k = rnd(len);
      Rec *old = lng[k];
      lng[k] = rec(old->n + 1, str10(i), old->x);
      free_rec(old);
    }
  }
  for (int64_t i = 0; i < len; ++i) {
    Rec *r = lng[i];
    n += r->n + (int64_t)strlen(r->s);
    if (r->x) n += r->x->n;
  }
  bench_report("long_short_mix", t, n == 5099739142LL);
  for (int64_t i = 0; i < len; ++i) {
    free_rec(lng[i]->x);
    free_rec(lng[i]);
  }
  free(lng);
  return 0;
}
