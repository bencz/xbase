/* bench/method_call_overhead.prg in C: 5,000,000 rounds of method calls with no argument, one and three arguments, an
 * inline method and a method that calls another through self; on an object whose class the compiler knows (direct
 * calls, which it may inline), then on one reached through a pointer the compiler cannot follow, each call through the
 * class's table of methods. */
#include "bench.h"

typedef struct Calc Calc;
typedef struct {
  int64_t (*zero)(Calc *self);
  int64_t (*one)(Calc *self, int64_t x);
  int64_t (*three)(Calc *self, int64_t x, int64_t y, int64_t z);
  int64_t (*twice)(Calc *self, int64_t x);
  int64_t (*chain)(Calc *self, int64_t x);
} Methods;

struct Calc {
  const Methods *m;
  int64_t base;
};

static int64_t calc_zero(Calc *self) { return self->base; }
static int64_t calc_one(Calc *self, int64_t x) {
  (void)self;
  return x;
}
static int64_t calc_three(Calc *self, int64_t x, int64_t y, int64_t z) {
  (void)self;
  return x + y + z - 5;
}
static int64_t calc_twice(Calc *self, int64_t x) {
  (void)self;
  return x + x;
}
static int64_t calc_chain(Calc *self, int64_t x) { return self->m->one(self, x) + self->m->one(self, x) + 1; }
static const Methods calc_methods = {calc_zero, calc_one, calc_three, calc_twice, calc_chain};

/* Chain() of the known class: its calls of One() resolved too. */
static int64_t known_chain(Calc *self, int64_t x) { return calc_one(self, x) + calc_one(self, x) + 1; }

int main(void) {
  long long t = bench_ms();
  Calc known = {&calc_methods, 1}, made = {&calc_methods, 1};
  Calc *o = &known, *volatile pu = &made; /* u is made by a macro: its class unknown to the compiler */
  int64_t n = 0;
  for (int64_t i = 1; i <= 5000000; ++i) {
    n += calc_zero(o) + calc_one(o, i) + calc_three(o, i, 2, 3) + calc_twice(o, i) + known_chain(o, i);
    Calc *u = pu;
    n += u->m->zero(u) + u->m->one(u, i) + u->m->three(u, i, 2, 3) + u->m->twice(u, i) + u->m->chain(u, i);
  }
  bench_report("method_call_overhead", t, n == 150000050000000LL);
  return 0;
}
