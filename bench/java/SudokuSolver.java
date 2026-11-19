// bench/sudoku_solver.prg in Java: recursion and arrays of booleans, five hard sudoku puzzles solved by backtracking
// (the first empty cell tries each digit its row, column and box allow), checked by a hash of the solved grids, 3
// times.
public class SudokuSolver {
  // Marks digit v as used, or free, in the row, column and box of cell p (0 to 80).
  static void mark(int p, int v, boolean on, boolean[] row, boolean[] col, boolean[] box) {
    int r = p / 9, c = p - p / 9 * 9;
    row[r * 9 + v - 1] = on;
    col[c * 9 + v - 1] = on;
    box[(r / 3 * 3 + c / 3) * 9 + v - 1] = on;
  }

  // Fills the empty cells from cell p (0 to 80) on; true when the grid is solved.
  static boolean solve(int p, int[] grid, boolean[] row, boolean[] col, boolean[] box) {
    while (p < 81 && grid[p] != 0) p++;
    if (p == 81) return true;
    int r = p / 9, c = p - r * 9, b = r / 3 * 3 + c / 3;
    for (int v = 1; v <= 9; v++) {
      if (row[r * 9 + v - 1] || col[c * 9 + v - 1] || box[b * 9 + v - 1]) continue;
      grid[p] = v;
      mark(p, v, true, row, col, box);
      if (solve(p + 1, grid, row, col, box)) return true;
      mark(p, v, false, row, col, box);
      grid[p] = 0;
    }
    return false;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    boolean ok = true;
    String[] puzzles = {
        "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4..",
        "6.....8.3.4.7.................5.4.7.3..2.....1.6.......2.....5.....8.6......1....",
        "48.3............71.2.......7.5....6....2..8.............1.76...3.....4......5....",
        "..53.....8......2..7..1.5..4....53...1..7...6..32...8..6.5....9..4....3......97..",
        "1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3.."};
    for (int rep = 1; rep <= 3; rep++) {
      long sum = 0;
      for (String puzzle : puzzles) {
        int[] grid = new int[81];
        boolean[] row = new boolean[81], col = new boolean[81], box = new boolean[81];
        for (int i = 0; i < 81; i++) {
          grid[i] = puzzle.charAt(i) == '.' ? 0 : puzzle.charAt(i) - '0';
          if (grid[i] != 0) mark(i, grid[i], true, row, col, box);
        }
        ok = ok && solve(0, grid, row, col, box);
        for (int i = 0; i < 81; i++) sum = (sum * 31 + grid[i]) & 0xFFFFFFF;
      }
      ok = ok && sum == 252679381;
    }
    Bench.report("sudoku_solver", t, ok);
  }
}
