// bench/fannkuch_redux.prg in Java: small integer arrays permuted and reversed in place, the pancake flips of every
// permutation of 9 elements (the checksum and the most flips), 4 times.
public class FannkuchRedux {
  public static void main(String[] args) {
    long t = System.nanoTime();
    int n = 9;
    boolean ok = true;
    for (int rep = 1; rep <= 4; rep++) {
      int[] perm = new int[n + 1], perm1 = new int[n + 1], count = new int[n + 1]; // 1-based, as the .prg's arrays
      for (int i = 1; i <= n; i++) perm1[i] = i - 1;
      long checksum = 0, maxFlips = 0, permCount = 0;
      int r = n;
      boolean done = false;
      while (!done) {
        while (r != 1) {
          count[r] = r;
          r--;
        }
        for (int i = 1; i <= n; i++) perm[i] = perm1[i];
        long flips = 0;
        int k = perm[1];
        while (k != 0) {
          int i = 1, j = k + 1;
          while (i < j) {
            int x = perm[i];
            perm[i] = perm[j];
            perm[j] = x;
            i++;
            j--;
          }
          flips++;
          k = perm[1];
        }
        maxFlips = Math.max(maxFlips, flips);
        checksum += (permCount & 1) == 0 ? flips : -flips;
        while (true) {
          if (r == n) {
            done = true;
            break;
          }
          int perm0 = perm1[1];
          for (int i = 1; i <= r; i++) perm1[i] = perm1[i + 1];
          perm1[r + 1] = perm0;
          count[r + 1]--;
          if (count[r + 1] > 0) break;
          r++;
        }
        permCount++;
      }
      ok = ok && checksum == 8629 && maxFlips == 30;
    }
    Bench.report("fannkuch_redux", t, ok);
  }
}
