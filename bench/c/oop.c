/* bench/oop.prg in C: objects as structs with a table of methods (the dispatch a class gives), one method calling its
 * parent's; method calls, variable reads and assignments in a loop of 3,000,000. */
#include "bench.h"

typedef struct Base Base;
typedef struct {
  void (*add)(Base *self, int64_t x);
} BaseMethods;
struct Base {
  const BaseMethods *m;
  int64_t total;
};
typedef struct {
  Base base;
  int64_t n;
} Counter;

static void base_add(Base *self, int64_t x) { self->total += x; }
static const BaseMethods base_methods = {base_add};
static void counter_add(Base *self, int64_t x) { base_methods.add(self, x); } /* Super:Add( x ) */
static const BaseMethods counter_methods = {counter_add};

int main(void) {
  long long t = bench_ms();
  Counter o = {{&counter_methods, 0}, 0};
  Counter *volatile p = &o; /* reached through a pointer, as an object reference */
  for (int64_t i = 1; i <= 3000000; ++i) {
    p->base.m->add(&p->base, i % 7);
    p->n = p->n + 1;
  }
  int64_t r = p->base.total + p->n;
  bench_report("oop", t, r == 11999997);
  return 0;
}
