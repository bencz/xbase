/* bench/closure_capture.prg in C: closures as a function pointer and the captured variables in a struct; 500,000
 * counters made by a function, each holding its own copy of a local and a parameter, each called 4 times; then
 * 2,000,000 calls of two closures that share one captured local (held by reference) and change it. */
#include "bench.h"
#include <stdlib.h>

typedef struct Block Block;
struct Block {
  int64_t (*eval)(Block *self, int64_t x);
};

typedef struct {
  Block block;
  int64_t count, step;
} CounterBlock;

typedef struct {
  Block block;
  int64_t *shared;
} SharedBlock;

static int64_t counter_eval(Block *self, int64_t x) {
  CounterBlock *c = (CounterBlock *)self;
  return c->count += c->step + x;
}

static Block *counter(int64_t step) {
  CounterBlock *c = malloc(sizeof *c);
  *c = (CounterBlock){{counter_eval}, 0, step};
  return &c->block;
}

static int64_t inc_eval(Block *self, int64_t x) { return *((SharedBlock *)self)->shared += x; }
static int64_t read_eval(Block *self, int64_t x) {
  (void)x;
  return *((SharedBlock *)self)->shared % 1000;
}

int main(void) {
  long long t = bench_ms();
  int64_t len = 500000, n = 0, shared = 0;
  Block **a = malloc((size_t)len * sizeof *a);
  for (int64_t i = 1; i <= len; ++i) a[i - 1] = counter(i % 10);
  for (int64_t j = 1; j <= 4; ++j)
    for (int64_t i = 0; i < len; ++i) n += a[i]->eval(a[i], j);
  SharedBlock inc = {{inc_eval}, &shared}, rd = {{read_eval}, &shared};
  Block *volatile binc = &inc.block, *volatile bread = &rd.block;
  for (int64_t i = 1; i <= 2000000; ++i) {
    Block *b = binc;
    b->eval(b, i % 7);
    b = bread;
    n += b->eval(b, 0);
  }
  bench_report("closure_capture", t, n == 1031500997 && shared == 5999997);
  for (int64_t i = 0; i < len; ++i) free(a[i]);
  free(a);
  return 0;
}
