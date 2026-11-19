// bench/knapsack.prg in Java: 0/1 knapsack by dynamic programming over one array of capacities: 200 items (weights
// 1 .. 1000 and values 1 .. 1000 from a linear congruential generator) into capacities up to 10,000, 10 times with
// different seeds; the best values are checked.
import java.util.Arrays;

public class Knapsack {
  public static void main(String[] args) {
    long t = System.nanoTime(), total = 0;
    int n = 200, cap = 10000;
    long[] best = new long[cap + 2], w = new long[n + 1], v = new long[n + 1];
    for (long rep = 1; rep <= 10; rep++) {
      long seed = rep;
      for (int i = 1; i <= n; i++) {
        seed = seed * 16807 % 2147483647;
        w[i] = seed / 65536 % 1000 + 1;
        seed = seed * 16807 % 2147483647;
        v[i] = seed / 65536 % 1000 + 1;
      }
      Arrays.fill(best, 0);
      for (int i = 1; i <= n; i++) {
        int wi = (int) w[i];
        long vi = v[i];
        for (int c = cap + 1; c >= wi + 1; c--) {
          long x = best[c - wi] + vi;
          if (x > best[c]) best[c] = x;
        }
      }
      total += best[cap + 1];
    }
    Bench.report("knapsack", t, total == 366516);
  }
}
