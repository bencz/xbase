/* bench/sudoku_solver.prg in C: five hard sudoku puzzles solved by backtracking (the first empty cell tries each
 * digit its row, column and box allow), checked by a hash of the solved grids, 3 times. */
#include "bench.h"
#include <stdbool.h>
#include <string.h>

static const char *const puzzles[] = {
  "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4..",
  "6.....8.3.4.7.................5.4.7.3..2.....1.6.......2.....5.....8.6......1....",
  "48.3............71.2.......7.5....6....2..8.............1.76...3.....4......5....",
  "..53.....8......2..7..1.5..4....53...1..7...6..32...8..6.5....9..4....3......97..",
  "1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3..",
};
static int64_t grid[81];
static bool rowUsed[81], colUsed[81], boxUsed[81];

static void mark(int p, int64_t v, bool on) {
  int r = p / 9, c = p % 9, b = r / 3 * 3 + c / 3;
  rowUsed[r * 9 + v - 1] = on;
  colUsed[c * 9 + v - 1] = on;
  boxUsed[b * 9 + v - 1] = on;
}

static bool solve(int p) {
  while (p < 81 && grid[p] != 0) ++p;
  if (p == 81) return true;
  int r = p / 9, c = p % 9, b = r / 3 * 3 + c / 3;
  for (int64_t v = 1; v <= 9; ++v) {
    if (rowUsed[r * 9 + v - 1] || colUsed[c * 9 + v - 1] || boxUsed[b * 9 + v - 1]) continue;
    grid[p] = v;
    mark(p, v, true);
    if (solve(p + 1)) return true;
    mark(p, v, false);
    grid[p] = 0;
  }
  return false;
}

int main(void) {
  long long t = bench_ms();
  int ok = 1;
  for (int rep = 1; rep <= 3; ++rep) {
    int64_t sum = 0;
    for (size_t k = 0; k < sizeof puzzles / sizeof puzzles[0]; ++k) {
      memset(rowUsed, 0, sizeof rowUsed);
      memset(colUsed, 0, sizeof colUsed);
      memset(boxUsed, 0, sizeof boxUsed);
      for (int i = 0; i < 81; ++i) {
        char ch = puzzles[k][i];
        grid[i] = ch == '.' ? 0 : ch - '0';
        if (grid[i] != 0) mark(i, grid[i], true);
      }
      bool solved = solve(0);
      ok = ok && solved;
      for (int i = 0; i < 81; ++i) sum = (sum * 31 + grid[i]) & 0xFFFFFFF;
    }
    ok = ok && sum == 252679381;
  }
  bench_report("sudoku_solver", t, ok);
  return 0;
}
