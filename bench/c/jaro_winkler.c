/* bench/jaro_winkler.prg in C: the Jaro-Winkler similarity of 300,000 pairs of 6 to 14 letters (the second one the
 * first with random changes), the sum of the doubles checked to 6 decimals. */
#include "bench.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char s[16], u[20];
} Pair;

static int64_t seed = 271;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static double jaro_winkler(const char *s1, const char *s2) {
  int64_t l1 = (int64_t)strlen(s1), l2 = (int64_t)strlen(s2), m = 0, trans = 0, prefix = 0;
  if (l1 == 0 || l2 == 0) return l1 == l2 ? 1 : 0;
  int64_t range = (l1 > l2 ? l1 : l2) / 2 - 1;
  if (range < 0) range = 0;
  char *a1 = calloc((size_t)l1, 1), *a2 = calloc((size_t)l2, 1);
  for (int64_t i = 1; i <= l1; ++i) {
    int64_t from = i - range > 1 ? i - range : 1, to = i + range < l2 ? i + range : l2;
    for (int64_t j = from; j <= to; ++j)
      if (!a2[j - 1] && s1[i - 1] == s2[j - 1]) {
        a1[i - 1] = a2[j - 1] = 1;
        ++m;
        break;
      }
  }
  if (m == 0) {
    free(a1);
    free(a2);
    return 0;
  }
  int64_t k = 1;
  for (int64_t i = 1; i <= l1; ++i)
    if (a1[i - 1]) {
      while (!a2[k - 1]) ++k;
      if (s1[i - 1] != s2[k - 1]) ++trans;
      ++k;
    }
  free(a1);
  free(a2);
  double jaro = ((double)m / (double)l1 + (double)m / (double)l2 + ((double)m - (double)trans / 2) / (double)m) / 3;
  int64_t shorter = l1 < l2 ? l1 : l2;
  while (prefix < 4 && prefix < shorter && s1[prefix] == s2[prefix]) ++prefix;
  return jaro + (double)prefix * 0.1 * (1 - jaro);
}

/* Stuff( u, k, n, c ): n characters at k replaced by c. */
static void stuff(char *u, int64_t k, int64_t n, char c) {
  size_t len = strlen(u);
  memmove(u + k, u + k - 1 + n, len - (size_t)(k - 1 + n) + 1);
  u[k - 1] = c;
}

static void make_pair(Pair *p, int64_t len) {
  for (int64_t i = 0; i < len; ++i) p->s[i] = (char)(65 + rnd(6));
  p->s[len] = '\0';
  strcpy(p->u, p->s);
  for (int i = 1; i <= 3; ++i) {
    int64_t k = rnd((int64_t)strlen(p->u)) + 1, n = rnd(2);
    stuff(p->u, k, n, (char)(65 + rnd(6)));
  }
}

int main(void) {
  int64_t n = 300000;
  Pair *pairs = malloc((size_t)n * sizeof *pairs);
  double sum = 0;
  for (int64_t i = 0; i < n; ++i) make_pair(&pairs[i], 6 + rnd(9));
  long long t = bench_ms();
  for (int64_t i = 0; i < n; ++i) sum += jaro_winkler(pairs[i].s, pairs[i].u);
  int ok = fabs(sum - 260943.0356007) < 0.000001 && fabs(jaro_winkler("MARTHA", "MARHTA") - 0.961111111) < 0.000000001;
  bench_report("jaro_winkler", t, ok);
  free(pairs);
  return 0;
}
