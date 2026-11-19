/* bench/n_queens.prg in C: the solutions of the 12 queens problem (backtracking over arrays of taken columns and
 * diagonals), 3 times. */
#include "bench.h"
#include <stdbool.h>

enum { N = 12 };
static bool col[N + 1], d1[2 * N], d2[2 * N];

static int64_t place(int row) {
  if (row > N) return 1;
  int64_t count = 0;
  for (int c = 1; c <= N; ++c)
    if (!col[c] && !d1[row + c - 1] && !d2[row - c + N]) {
      col[c] = d1[row + c - 1] = d2[row - c + N] = true;
      count += place(row + 1);
      col[c] = d1[row + c - 1] = d2[row - c + N] = false;
    }
  return count;
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 3; ++rep) ok = ok && place(1) == 14200;
  bench_report("n_queens", t, ok);
  return 0;
}
