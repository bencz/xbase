/* bench/bitlookup.prg in C (the original program, checked for every pair): the bitwise AND from a table of nibbles
 * built by nested macros, IOR and XOR made from it by the same macros, for every pair a, b in 0 .. 999: 7 calls of
 * AND() a pair, which the compiler may neither inline nor merge (noinline, noipa), as xbase calls its function. */
#include "bench.h"
#include <stdint.h>

#define XOR(a, b) (a + b - 2 * AND(a, b))
#define IOR(a, b) XOR(XOR(a, b), AND(a, b))

static const uint16_t andlookup[256] = {
#define C4(a, b) ((a) & (b)), ((a) & (b + 1)), ((a) & (b + 2)), ((a) & (b + 3))
#define L(a) C4(a, 0), C4(a, 4), C4(a, 8), C4(a, 12)
#define L4(a) L(a), L(a + 1), L(a + 2), L(a + 3)
    L4(0), L4(4), L4(8), L4(12)
#undef C4
#undef L
#undef L4
};

__attribute__((noinline, noipa)) static int AND(int a, int b) {
  int r = 0;
  for (int i = 0; i < 16; i += 4) {
    r = r / 16 + andlookup[(a % 16) * 16 + (b % 16)] * 4096;
    a /= 16;
    b /= 16;
  }
  return r;
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int a = 0; a < 1000 && ok; ++a)
    for (int b = 0; b < 1000; ++b)
      if (AND(a, b) != (a & b) || IOR(a, b) != (a | b) || XOR(a, b) != (a ^ b)) {
        ok = 0;
        break;
      }
  bench_report("bitlookup", t, ok);
  return 0;
}
