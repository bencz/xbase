/* bench/map_filter_reduce.prg in C: map, filter and reduce over arrays with closures (a function pointer and the
 * captured round): an array of 1,000,000 numbers mapped into a new array, the result filtered into another (grown one
 * element at a time) and reduced to a sum; 8 rounds. */
#include "bench.h"
#include <stdlib.h>

typedef struct {
  int64_t (*eval)(int64_t r, int64_t x, int64_t y);
  int64_t r;
} Block;

typedef struct {
  int64_t *items, len, cap;
} Array;

static int64_t mapper(int64_t r, int64_t x, int64_t y) {
  (void)y;
  return x * r + 1;
}
static int64_t third(int64_t r, int64_t x, int64_t y) {
  (void)r;
  (void)y;
  return x % 3 == 0;
}
static int64_t add(int64_t r, int64_t acc, int64_t x) {
  (void)r;
  return acc + x;
}

static Array map(const Array *a, const Block *b) {
  Array out = {malloc((size_t)a->len * sizeof(int64_t)), a->len, a->len};
  for (int64_t i = 0; i < a->len; ++i) out.items[i] = b->eval(b->r, a->items[i], 0);
  return out;
}

static Array filter(const Array *a, const Block *b) {
  Array out = {0};
  for (int64_t i = 0; i < a->len; ++i)
    if (b->eval(b->r, a->items[i], 0)) {
      if (out.len == out.cap) {
        out.cap = out.cap ? out.cap * 2 : 4;
        out.items = realloc(out.items, (size_t)out.cap * sizeof *out.items);
      }
      out.items[out.len++] = a->items[i];
    }
  return out;
}

static int64_t reduce(const Array *a, const Block *b, int64_t acc) {
  for (int64_t i = 0; i < a->len; ++i) acc = b->eval(b->r, acc, a->items[i]);
  return acc;
}

int main(void) {
  Array a = {malloc(1000000 * sizeof(int64_t)), 1000000, 1000000};
  int64_t n = 0;
  for (int64_t i = 1; i <= a.len; ++i) a.items[i - 1] = i % 1000;
  long long t = bench_ms();
  for (int64_t r = 1; r <= 8; ++r) {
    Block bm = {mapper, r}, bf = {third, r}, br = {add, r};
    const Block *volatile pm = &bm, *volatile pf = &bf, *volatile pr = &br;
    Array m = map(&a, pm), f = filter(&m, pf);
    n += reduce(&f, pr, 0);
    free(m.items);
    free(f.items);
  }
  bench_report("map_filter_reduce", t, n == 4492503000LL);
  free(a.items);
  return 0;
}
