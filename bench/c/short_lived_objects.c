/* bench/short_lived_objects.prg in C: 2,000,000 small vectors (allocated structs) made by a constructor, each added to
 * a running sum by a method that returns a new vector, so three objects are made and as many freed per step. */
#include "bench.h"
#include <stdlib.h>

typedef struct {
  int64_t x, y;
} Vec;

/* noinline and noipa: each vector is allocated and freed, as an object is made and dropped. */
__attribute__((noinline, noipa)) static Vec *vec_new(int64_t x, int64_t y) {
  Vec *v = malloc(sizeof *v);
  v->x = x;
  v->y = y;
  return v;
}
static Vec *vec_add(const Vec *v, const Vec *o) { return vec_new(v->x + o->x, v->y + o->y); }
static Vec *vec_scaled(const Vec *v, int64_t k) { return vec_new(v->x * k, v->y * k); }
static int64_t vec_dot(const Vec *v, const Vec *o) { return v->x * o->x + v->y * o->y; }

int main(void) {
  long long t = bench_ms();
  Vec *s = vec_new(0, 0);
  int64_t n = 0;
  for (int64_t i = 1; i <= 2000000; ++i) {
    Vec *v = vec_new(i % 10, i % 7 - 3), *sum = vec_add(s, v);
    free(s);
    s = sum;
    Vec *zero = vec_scaled(s, 0);
    n += vec_dot(v, zero) + v->x;
    free(zero);
    free(v);
  }
  int ok = s->x == 9000000 && s->y == -3 && n == 9000000;
  bench_report("short_lived_objects", t, ok);
  free(s);
  return 0;
}
