/* bench/levenshtein.prg in C: the Levenshtein distance of 10,000 pairs of strings of 30 to 60 letters (the second one
 * the first with random edits), by dynamic programming over two rows allocated for each pair. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  char s[64], u[128];
} Pair;

static int64_t seed = 1001;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static int64_t min2(int64_t a, int64_t b) { return a < b ? a : b; }

static int64_t distance(const char *s, const char *u) {
  int64_t m = (int64_t)strlen(s), n = (int64_t)strlen(u);
  int64_t *prev = malloc((size_t)(n + 1) * sizeof *prev), *cur = malloc((size_t)(n + 1) * sizeof *cur);
  for (int64_t j = 0; j <= n; ++j) prev[j] = j;
  for (int64_t i = 1; i <= m; ++i) {
    cur[0] = i;
    char c = s[i - 1];
    for (int64_t j = 1; j <= n; ++j)
      cur[j] = min2(min2(prev[j], cur[j - 1]) + 1, prev[j - 1] + (c == u[j - 1] ? 0 : 1));
    int64_t *swap = prev;
    prev = cur;
    cur = swap;
  }
  int64_t d = prev[n];
  free(prev);
  free(cur);
  return d;
}

static void make_pair(Pair *p, int64_t len) {
  size_t k = 0;
  for (int64_t i = 0; i < len; ++i) p->s[i] = (char)(97 + rnd(4));
  p->s[len] = '\0';
  for (int64_t i = 0; i < len; ++i) {
    int64_t r = rnd(10);
    if (r == 0)
      p->u[k++] = (char)(97 + rnd(4));
    else if (r == 1) {
      p->u[k++] = p->s[i];
      p->u[k++] = (char)(97 + rnd(4));
    } else if (r != 2)
      p->u[k++] = p->s[i];
  }
  p->u[k] = '\0';
}

int main(void) {
  int64_t count = 10000, n = 0;
  Pair *pairs = malloc((size_t)count * sizeof *pairs);
  for (int64_t i = 0; i < count; ++i) make_pair(&pairs[i], 30 + rnd(31));
  long long t = bench_ms();
  for (int64_t i = 0; i < count; ++i) n += distance(pairs[i].s, pairs[i].u);
  int ok = n == 108309 && distance("kitten", "sitting") == 3;
  bench_report("levenshtein", t, ok);
  free(pairs);
  return 0;
}
