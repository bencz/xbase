/* bench/inheritance_depth.prg in C: a line of 8 classes, each inheriting the one before (structs that begin with their
 * parent, a table of methods each), on an object of the last reached through a pointer the compiler cannot follow:
 * 4,000,000 rounds of a method each class overrides and that calls its parent's down to the first (8 calls, which the
 * compiler may neither inline nor fold: noinline, noipa), a method only the first class has, and variables declared
 * by the first, a middle and the last class (read, and the first one assigned). */
#include "bench.h"

typedef struct TL0 TL0;
typedef struct {
  int64_t (*depth)(TL0 *self);
  int64_t (*base)(TL0 *self);
} Methods;

struct TL0 {
  const Methods *m;
  int64_t v0;
};
typedef struct {
  TL0 super;
  int64_t v1;
} TL1;
typedef struct {
  TL1 super;
  int64_t v2;
} TL2;
typedef struct {
  TL2 super;
  int64_t v3;
} TL3;
typedef struct {
  TL3 super;
  int64_t v4;
} TL4;
typedef struct {
  TL4 super;
  int64_t v5;
} TL5;
typedef struct {
  TL5 super;
  int64_t v6;
} TL6;
typedef struct {
  TL6 super;
  int64_t v7;
} TL7;

static int64_t base0(TL0 *self) { return self->v0 + 100; }
__attribute__((noinline, noipa)) static int64_t depth0(TL0 *self) {
  (void)self;
  return 1;
}
static const Methods tl0 = {depth0, base0};
__attribute__((noinline, noipa)) static int64_t depth1(TL0 *self) { return tl0.depth(self) + 2; }
static const Methods tl1 = {depth1, base0};
__attribute__((noinline, noipa)) static int64_t depth2(TL0 *self) { return tl1.depth(self) + 3; }
static const Methods tl2 = {depth2, base0};
__attribute__((noinline, noipa)) static int64_t depth3(TL0 *self) { return tl2.depth(self) + 4; }
static const Methods tl3 = {depth3, base0};
__attribute__((noinline, noipa)) static int64_t depth4(TL0 *self) { return tl3.depth(self) + 5; }
static const Methods tl4 = {depth4, base0};
__attribute__((noinline, noipa)) static int64_t depth5(TL0 *self) { return tl4.depth(self) + 6; }
static const Methods tl5 = {depth5, base0};
__attribute__((noinline, noipa)) static int64_t depth6(TL0 *self) { return tl5.depth(self) + 7; }
static const Methods tl6 = {depth6, base0};
__attribute__((noinline, noipa)) static int64_t depth7(TL0 *self) { return tl6.depth(self) + 8; }
static const Methods tl7 = {depth7, base0};

int main(void) {
  long long t = bench_ms();
  TL7 obj = {{{{{{{{&tl7, 0}, 1}, 2}, 3}, 4}, 5}, 6}, 7};
  TL7 *volatile p = &obj; /* made by a macro: its class unknown to the compiler */
  int64_t n = 0;
  for (int64_t i = 1; i <= 4000000; ++i) {
    TL7 *o = p;
    TL0 *o0 = &o->super.super.super.super.super.super.super;
    n += o0->m->depth(o0) + o0->m->base(o0) + o0->v0 + o->super.super.super.v4 + o->v7;
    o0->v0 = i % 3;
  }
  bench_report("inheritance_depth", t, n == 4000000LL * (36 + 100 + 11) + 2 * 3999999LL);
  return 0;
}
