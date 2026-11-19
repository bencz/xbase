/* bench/codeblock_eval.prg in C: 10,000,000 calls of each of three closures (a function pointer and its captured
 * variables) kept in an array: no argument, two arguments, one that reads a captured local; the results summed. */
#include "bench.h"

typedef struct Block Block;
struct Block {
  int64_t (*eval)(const Block *self, int64_t x, int64_t y);
  int64_t base;
};

static int64_t seven(const Block *self, int64_t x, int64_t y) {
  (void)self;
  (void)x;
  (void)y;
  return 7;
}
static int64_t twice_plus(const Block *self, int64_t x, int64_t y) {
  (void)self;
  return x * 2 + y;
}
static int64_t plus_base(const Block *self, int64_t x, int64_t y) {
  (void)y;
  return x + self->base;
}

static void blocks(Block *b, int64_t base) {
  b[0] = (Block){seven, 0};
  b[1] = (Block){twice_plus, 0};
  b[2] = (Block){plus_base, base};
}

int main(void) {
  long long t = bench_ms();
  int64_t n = 0;
  Block b[3];
  blocks(b, 3);
  Block *volatile pb = b;
  for (int64_t i = 1; i <= 10000000; ++i) {
    const Block *a = pb;
    n += a[0].eval(&a[0], 0, 0) + a[1].eval(&a[1], i % 100, 1) + a[2].eval(&a[2], i % 10, 0);
  }
  bench_report("codeblock_eval", t, n == 1145000000);
  return 0;
}
