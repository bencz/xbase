/* bench/towers_of_hanoi.prg in C: 20 disks moved between three pegs (arrays with a height each), every move checked
 * to put a smaller disk on a larger one, 4 times. */
#include "bench.h"

enum { DISKS = 20 };
static int64_t peg[3][DISKS], top[3], bad;

static int64_t hanoi(int64_t n, int from, int to, int via) {
  int64_t moves = 0;
  if (n > 0) {
    moves = hanoi(n - 1, from, via, to);
    int64_t d = peg[from][top[from] - 1];
    --top[from];
    if (top[to] > 0 && peg[to][top[to] - 1] < d) ++bad;
    peg[to][top[to]] = d;
    ++top[to];
    moves += 1 + hanoi(n - 1, via, to, from);
  }
  return moves;
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 4; ++rep) {
    for (int i = 0; i < DISKS; ++i) peg[0][i] = DISKS - i;
    top[0] = DISKS;
    top[1] = 0;
    top[2] = 0;
    bad = 0;
    int64_t moves = hanoi(DISKS, 0, 2, 1);
    ok = ok && moves == (1 << DISKS) - 1 && bad == 0 && top[2] == DISKS;
    for (int i = 0; i < DISKS; ++i) ok = ok && peg[2][i] == DISKS - i;
  }
  bench_report("towers_of_hanoi", t, ok);
  return 0;
}
