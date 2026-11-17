/* bench/generator.prg in C: an iterator of 3,000,000 numbers (its state in a struct, one step a call, through a
 * pointer the compiler cannot follow, as a generator resumes) and a second one that takes the even ones from the
 * first. */
#include "bench.h"

typedef struct {
  int64_t next, last;
} Range;
static int range_next(Range *r, int64_t *out) {
  if (r->next > r->last) return 0;
  *out = r->next++;
  return 1;
}

static int (*volatile range_step)(Range *, int64_t *) = range_next;

typedef struct {
  Range *source;
} Evens;
static int evens_next(Evens *e, int64_t *out) {
  int64_t x;
  while (range_step(e->source, &x))
    if (x % 2 == 0) {
      *out = x;
      return 1;
    }
  return 0;
}

int main(void) {
  long long t = bench_ms(), s = 0;
  Range r = {1, 3000000};
  Evens e = {&r};
  int (*volatile evens_step)(Evens *, int64_t *) = evens_next;
  int64_t x;
  while (evens_step(&e, &x)) s += x;
  bench_report("generator", t, s == 2250001500000LL);
  return 0;
}
