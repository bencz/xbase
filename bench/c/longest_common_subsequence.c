/* bench/longest_common_subsequence.prg in C: the length of the longest common subsequence of 600 pairs of 200 to 300
 * letters from a 4-letter alphabet (the second related to the first), by dynamic programming over two rows, the
 * letters copied into arrays first. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  char s[304], u[608];
} Pair;

static int64_t seed = 31;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static int64_t lcs(const char *s, const char *u) {
  int64_t m = (int64_t)strlen(s), n = (int64_t)strlen(u);
  char *as = malloc((size_t)m), *au = malloc((size_t)n);
  int64_t *prev = calloc((size_t)(n + 1), sizeof *prev), *cur = calloc((size_t)(n + 1), sizeof *cur);
  memcpy(as, s, (size_t)m);
  memcpy(au, u, (size_t)n);
  for (int64_t i = 0; i < m; ++i) {
    char c = as[i];
    for (int64_t j = 1; j <= n; ++j)
      if (c == au[j - 1])
        cur[j] = prev[j - 1] + 1;
      else
        cur[j] = prev[j] > cur[j - 1] ? prev[j] : cur[j - 1];
    int64_t *swap = prev;
    prev = cur;
    cur = swap;
  }
  int64_t r = prev[n];
  free(as);
  free(au);
  free(prev);
  free(cur);
  return r;
}

static void make_pair(Pair *p, int64_t len) {
  size_t k = 0;
  for (int64_t i = 0; i < len; ++i) p->s[i] = (char)(65 + rnd(4));
  p->s[len] = '\0';
  for (int64_t i = 0; i < len; ++i) {
    if (rnd(4) != 0) p->u[k++] = p->s[i];
    if (rnd(5) == 0) p->u[k++] = (char)(65 + rnd(4));
  }
  p->u[k] = '\0';
}

int main(void) {
  int64_t count = 600, n = 0;
  Pair *pairs = malloc((size_t)count * sizeof *pairs);
  for (int64_t i = 0; i < count; ++i) make_pair(&pairs[i], 200 + rnd(101));
  long long t = bench_ms();
  for (int64_t i = 0; i < count; ++i) n += lcs(pairs[i].s, pairs[i].u);
  int ok = n == 118188 && lcs("ABCBDAB", "BDCABA") == 4;
  bench_report("longest_common_subsequence", t, ok);
  free(pairs);
  return 0;
}
