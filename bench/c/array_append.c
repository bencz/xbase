/* bench/array_append.prg in C: 5 arrays of 2,000,000 numbers grown one element at a time from empty (the store
 * doubling as it fills), then summed. */
#include "bench.h"
#include <stdlib.h>

typedef struct {
  int64_t *items, len, cap;
} Array;

static void aadd(Array *a, int64_t x) {
  if (a->len == a->cap) {
    a->cap = a->cap ? a->cap * 2 : 4;
    a->items = realloc(a->items, (size_t)a->cap * sizeof *a->items);
  }
  a->items[a->len++] = x;
}

int main(void) {
  long long t = bench_ms();
  int64_t sum = 0, last = 0;
  for (int64_t r = 1; r <= 5; ++r) {
    Array a = {0};
    for (int64_t i = 1; i <= 2000000; ++i) aadd(&a, i + r);
    for (int64_t i = 0; i < a.len; ++i) sum += a.items[i];
    last = a.len;
    free(a.items);
  }
  bench_report("array_append", t, sum == 10000035000000LL && last == 2000000);
  return 0;
}
