/* bench/indexacao_simulada.prg in C: an index kept in memory, as an order is: 150,000 entries (an 8-digit key and the
 * record number in one string each) made from a fixed seed, then timed: sorted with the library's sort, 100,000 seeks
 * by binary search (the first entry not below the key, compared over the key's length; a hit when its key is the one
 * sought), 3,000 entries inserted in place (binary search, the array grown by one and the entries after moved), and
 * the order checked. */
#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int64_t seed = 99991;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static int by_text(const void *p, const void *q) { return strcmp(*(char *const *)p, *(char *const *)q); }

/* The first position (1-based) whose entry is not below k over k's length; len + 1 when there is none. */
static int64_t lower_bound(char **a, int64_t len, const char *k) {
  int64_t lo = 1, hi = len + 1;
  size_t klen = strlen(k);
  while (lo < hi) {
    int64_t m = (lo + hi) / 2;
    if (strncmp(a[m - 1], k, klen) < 0)
      lo = m + 1;
    else
      hi = m;
  }
  return lo;
}

int main(void) {
  int64_t recs = 150000, len = recs, hits = 0, sum = 0, chk = 0;
  char **a = malloc((size_t)len * sizeof *a), k[24];
  int ok = 1;
  for (int64_t i = 1; i <= recs; ++i) {
    a[i - 1] = malloc(16);
    snprintf(a[i - 1], 16, "C%08lld%06lld", (long long)rnd(10000000), (long long)i);
  }
  long long t = bench_ms();
  qsort(a, (size_t)len, sizeof *a, by_text);
  for (int64_t i = 1; i <= 100000; ++i) {
    snprintf(k, sizeof k, "C%08lld", (long long)rnd(10000000));
    int64_t p = lower_bound(a, len, k);
    if (p <= len && strncmp(a[p - 1], k, 9) == 0) {
      ++hits;
      sum += strtoll(a[p - 1] + 9, NULL, 10);
    }
  }
  for (int64_t i = 1; i <= 3000; ++i) {
    char *e = malloc(16);
    snprintf(e, 16, "C%08lld%06lld", (long long)rnd(10000000), (long long)(recs + i));
    int64_t p = lower_bound(a, len, e);
    a = realloc(a, (size_t)(len + 1) * sizeof *a);
    ++len;
    memmove(a + p, a + p - 1, (size_t)(len - p) * sizeof *a);
    a[p - 1] = e;
  }
  for (int64_t i = 2; i <= len; ++i) ok = ok && strcmp(a[i - 2], a[i - 1]) < 0;
  for (int64_t i = 1; i <= len; i += 997) chk = (chk * 31 + strtoll(a[i - 1] + 9, NULL, 10)) % 1000000007;
  ok = ok && hits == 1426 && sum == 107771969 && chk == 409559053 && strcmp(a[0], "C00000040027737") == 0 &&
       strcmp(a[len - 1], "C09999947069110") == 0;
  bench_report("indexacao_simulada", t, ok);
  for (int64_t i = 0; i < len; ++i) free(a[i]);
  free(a);
  return 0;
}
