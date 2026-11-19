/* bench/array_growth.prg in C: 300,000 arrays grown one element at a time to 1 to 64 elements, enlarged to twice
 * that, cut to half, an element inserted in front and removed (the elements moved as AIns() and ADel() move them);
 * one in 100 kept to the end, the others freed at once. */
#include "bench.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  int64_t *items, len, cap;
} Array;

static int64_t seed = 3;
static int64_t rnd(int64_t n) {
  seed = seed * 16807 % 2147483647;
  return seed % n;
}

static void reserve(Array *a, int64_t n) {
  if (n <= a->cap) return;
  while (a->cap < n) a->cap = a->cap ? a->cap * 2 : 4;
  a->items = realloc(a->items, (size_t)a->cap * sizeof *a->items);
}

static void aadd(Array *a, int64_t x) {
  reserve(a, a->len + 1);
  a->items[a->len++] = x;
}

/* New elements are empty (0 stands for NIL: the program never reads one). */
static void asize(Array *a, int64_t n) {
  reserve(a, n);
  if (n > a->len) memset(a->items + a->len, 0, (size_t)(n - a->len) * sizeof *a->items);
  a->len = n;
}

static void ains(Array *a, int64_t pos) {
  memmove(a->items + pos, a->items + pos - 1, (size_t)(a->len - pos) * sizeof *a->items);
  a->items[pos - 1] = 0;
}

static void adel(Array *a, int64_t pos) {
  memmove(a->items + pos - 1, a->items + pos, (size_t)(a->len - pos) * sizeof *a->items);
  a->items[a->len - 1] = 0;
}

int main(void) {
  long long t = bench_ms();
  Array *kept = malloc(3000 * sizeof *kept);
  int64_t nkept = 0, n = 0;
  for (int64_t i = 1; i <= 300000; ++i) {
    Array a = {0};
    int64_t size = rnd(64) + 1;
    for (int64_t j = 1; j <= size; ++j) aadd(&a, j * 2);
    asize(&a, size * 2);
    a.items[size * 2 - 1] = i % 1000;
    n += a.items[size * 2 - 1];
    asize(&a, (size + 1) / 2);
    ains(&a, 1);
    a.items[0] = size;
    adel(&a, 1);
    a.items[a.len - 1] = i % 7;
    n += a.len + a.items[a.len - 1] + (a.len > 1 ? a.items[0] : 0);
    if (i % 100 == 0)
      kept[nkept++] = a;
    else
      free(a.items);
  }
  for (int64_t i = 0; i < nkept; ++i) n += kept[i].items[0];
  bench_report("array_growth", t, n == 156283982 && nkept == 3000);
  for (int64_t i = 0; i < nkept; ++i) free(kept[i].items);
  free(kept);
  return 0;
}
