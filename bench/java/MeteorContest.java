// bench/meteor_contest.prg in Java: bit masks and a recursive search: every way to fill the meteor contest's board
// (5 x 10 hexagonal cells, odd rows shifted half a cell right) with its 10 pieces of 5 cells. The pieces are read from
// a solution; their placements (12 orientations, every position, duplicates dropped through a HashSet) are 64-bit
// masks listed by piece and lowest cell. The search fills the lowest empty cell first and drops a placement that
// leaves an empty cell walled in among the next 10. The board turned half a turn is the same board, with cell i at
// 49 - i: piece 3, which no half turn leaves the same, takes only 6 of its orientations, one of each such pair, and
// every solution found counts for itself and its turned image (the reversed string). Checked: 2098 solutions, and the
// smallest and largest as strings of 50 digits.
import java.util.ArrayList;
import java.util.HashSet;

public class MeteorContest {
  static final int COLS = 5, ROWS = 10, CELLS = 50, HALVED = 3, WINDOW = 10;

  static long[][] mask = new long[10][];
  static int[][][] cells = new int[10][][];
  static int[][][] by = new int[10][CELLS][];
  static long[] neighbors = new long[CELLS];
  static int[] placed = new int[10];
  static long solutions;
  static String best, worst;

  // Offset coordinates (odd rows shifted right) of a cell (from 0) to axial ones { q, r }, and back; -1 off the board.
  static int[] toAxial(int cell) {
    int row = cell / COLS;
    return new int[] {cell - row * COLS - (row - (row & 1)) / 2, row};
  }

  static int toCell(int q, int r) {
    int col = q + (r - (r & 1)) / 2;
    if (r < 0 || r >= ROWS || col < 0 || col >= COLS) return -1;
    return r * COLS + col;
  }

  static void makePlacements(String shape) {
    int[] dq = {1, -1, 0, 0, 1, -1}, dr = {0, 0, 1, -1, -1, 1};
    for (int c = 0; c < CELLS; c++) {
      int[] at = toAxial(c);
      for (int i = 0; i < 6; i++) {
        int x = toCell(at[0] + dq[i], at[1] + dr[i]);
        if (x >= 0) neighbors[c] |= 1L << x;
      }
    }
    for (int p = 0; p < 10; p++) {
      ArrayList<Long> masks = new ArrayList<>();
      ArrayList<int[]> places = new ArrayList<>();
      ArrayList<ArrayList<Integer>> byCell = new ArrayList<>();
      for (int c = 0; c < CELLS; c++) byCell.add(new ArrayList<>());
      ArrayList<int[]> pts = new ArrayList<>();
      for (int c = 0; c < CELLS; c++)
        if (shape.charAt(c) - '0' == p) pts.add(toAxial(c));
      HashSet<Long> seen = new HashSet<>();
      for (int o = 0; o <= 11; o++) {
        for (int at = 0; at < CELLS; at++) {
          if (p == HALVED && o % 6 >= 3) break;
          int[] a = toAxial(at), place = new int[5];
          boolean ok = true;
          for (int i = 0; i < 5; i++) {
            int x = toCell(pts.get(i)[0] - pts.get(0)[0] + a[0], pts.get(i)[1] - pts.get(0)[1] + a[1]);
            place[i] = x;
            if (x < 0) {
              ok = false;
              break;
            }
          }
          if (ok) {
            long m = 0;
            int low = CELLS;
            for (int i = 0; i < 5; i++) {
              m |= 1L << place[i];
              low = Math.min(low, place[i]);
            }
            if (seen.add(m)) {
              masks.add(m);
              places.add(place);
              byCell.get(low).add(masks.size() - 1);
            }
          }
        }
        // The next orientation: a turn of 60 degrees, and after the sixth a mirror image.
        for (int[] pt : pts) {
          int q = pt[0];
          if (o == 5) {
            pt[0] = pt[1];
            pt[1] = q;
          } else {
            pt[0] = -pt[1];
            pt[1] = q + pt[1];
          }
        }
      }
      mask[p] = masks.stream().mapToLong(Long::longValue).toArray();
      cells[p] = places.toArray(new int[0][]);
      for (int c = 0; c < CELLS; c++) by[p][c] = byCell.get(c).stream().mapToInt(Integer::intValue).toArray();
    }
  }

  // Whether an empty cell among the WINDOW from cell has no empty neighbor.
  static boolean walledIn(long board, int cell) {
    for (int c = cell; c <= Math.min(cell + WINDOW, CELLS) - 1; c++)
      if (((board >> c) & 1) == 0 && (board & neighbors[c]) == neighbors[c]) return true;
    return false;
  }

  static void record(String sol) {
    if (solutions == 0 || sol.compareTo(best) < 0) best = sol;
    if (solutions == 0 || sol.compareTo(worst) > 0) worst = sol;
    solutions++;
  }

  static void solve(long board, int cell, int depth) {
    while (((board >> cell) & 1) == 1) cell++;
    if (depth == 10) {
      char[] sol = new char[CELLS];
      for (int p = 0; p < 10; p++)
        for (int i : cells[p][placed[p]]) sol[i] = (char) ('0' + p);
      StringBuilder turned = new StringBuilder();
      for (int i = CELLS - 1; i >= 0; i--) turned.append(sol[i]);
      record(new String(sol));
      record(turned.toString());
      return;
    }
    for (int p = 0; p < 10; p++) {
      if (placed[p] < 0) {
        for (int k : by[p][cell]) {
          long next = board | mask[p][k];
          if ((board & mask[p][k]) == 0 && !walledIn(next, cell)) {
            placed[p] = k;
            solve(next, cell, depth + 1);
          }
        }
        placed[p] = -1;
      }
    }
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    makePlacements("99998966856688568255777257472014220144031400311333");
    java.util.Arrays.fill(placed, -1);
    solve(0, 0, 0);
    Bench.report("meteor_contest", t, solutions == 2098
        && best.equals("00001222012661126155865558633348893448934747977799")
        && worst.equals("99998966856688568255777257472014220144031400311333"));
  }
}
