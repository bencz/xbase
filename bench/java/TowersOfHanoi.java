// bench/towers_of_hanoi.prg in Java: 20 disks moved between three pegs (arrays with a height each), every move
// checked to put a smaller disk on a larger one, 4 times.
public class TowersOfHanoi {
  static long hanoi(int n, int from, int to, int via, long[][] peg, int[] top, long[] bad) {
    long moves = 0;
    if (n > 0) {
      moves = hanoi(n - 1, from, via, to, peg, top, bad);
      long d = peg[from][top[from] - 1];
      top[from]--;
      if (top[to] > 0 && peg[to][top[to] - 1] < d) bad[0]++;
      top[to]++;
      peg[to][top[to] - 1] = d;
      moves += 1 + hanoi(n - 1, via, to, from, peg, top, bad);
    }
    return moves;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    int nDisks = 20;
    boolean ok = true;
    for (int rep = 1; rep <= 4; rep++) {
      long[][] peg = new long[3][nDisks];
      for (int i = 1; i <= nDisks; i++) peg[0][i - 1] = nDisks - i + 1;
      int[] top = {nDisks, 0, 0};
      long[] bad = {0};
      long moves = hanoi(nDisks, 0, 2, 1, peg, top, bad);
      ok = ok && moves == (1L << nDisks) - 1 && bad[0] == 0 && top[2] == nDisks;
      for (int i = 1; i <= nDisks; i++) ok = ok && peg[2][i - 1] == nDisks - i + 1;
    }
    Bench.report("towers_of_hanoi", t, ok);
  }
}
