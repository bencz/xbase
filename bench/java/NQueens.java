// bench/n_queens.prg in Java: backtracking over arrays of booleans: the solutions of the 12 queens problem (taken
// columns and diagonals), 3 times.
public class NQueens {
  // The solutions with queens on rows 1 to row - 1 as col and the diagonals d1 and d2 say (indexes from 1).
  static long place(int row, int n, boolean[] col, boolean[] d1, boolean[] d2) {
    long count = 0;
    if (row > n) return 1;
    for (int c = 1; c <= n; c++) {
      if (!col[c] && !d1[row + c - 1] && !d2[row - c + n]) {
        col[c] = true;
        d1[row + c - 1] = true;
        d2[row - c + n] = true;
        count += place(row + 1, n, col, d1, d2);
        col[c] = false;
        d1[row + c - 1] = false;
        d2[row - c + n] = false;
      }
    }
    return count;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    int n = 12;
    boolean ok = true;
    for (int rep = 1; rep <= 3; rep++)
      ok = ok && place(1, n, new boolean[n + 1], new boolean[2 * n], new boolean[2 * n]) == 14200;
    Bench.report("n_queens", t, ok);
  }
}
