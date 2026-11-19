// bench/jaro_winkler.prg in Java: string similarity, the Jaro-Winkler similarity of 300,000 pairs of 6 to 14 letters
// (the second one the first with random changes), the sum of the doubles checked to 6 decimals.
public class JaroWinkler {
  static long seed = 271;
  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  static double jaroWinkler(String s1, String s2) {
    int l1 = s1.length(), l2 = s2.length(), m = 0, trans = 0, prefix = 0;
    if (l1 == 0 || l2 == 0) return l1 == l2 ? 1 : 0;
    int range = Math.max(Math.max(l1, l2) / 2 - 1, 0);
    boolean[] a1 = new boolean[l1], a2 = new boolean[l2];
    for (int i = 1; i <= l1; i++) {
      for (int j = Math.max(1, i - range); j <= Math.min(l2, i + range); j++) {
        if (!a2[j - 1] && s1.charAt(i - 1) == s2.charAt(j - 1)) {
          a1[i - 1] = true;
          a2[j - 1] = true;
          m++;
          break;
        }
      }
    }
    if (m == 0) return 0;
    int k = 1;
    for (int i = 1; i <= l1; i++) {
      if (a1[i - 1]) {
        while (!a2[k - 1]) k++;
        if (s1.charAt(i - 1) != s2.charAt(k - 1)) trans++;
        k++;
      }
    }
    double jaro = ((double) m / l1 + (double) m / l2 + (m - trans / 2.0) / m) / 3;
    while (prefix < 4 && prefix < Math.min(l1, l2) && s1.charAt(prefix) == s2.charAt(prefix)) prefix++;
    return jaro + prefix * 0.1 * (1 - jaro);
  }

  static String[] pair(long len) {
    StringBuilder s = new StringBuilder();
    for (long i = 1; i <= len; i++) s.append((char) (65 + rand(6)));
    StringBuilder u = new StringBuilder(s);
    for (int i = 1; i <= 3; i++) {
      int k = (int) rand(u.length()) + 1;
      int del = (int) rand(2);
      u.replace(k - 1, Math.min(k - 1 + del, u.length()), String.valueOf((char) (65 + rand(6))));
    }
    return new String[] {s.toString(), u.toString()};
  }

  public static void main(String[] args) {
    String[][] pairs = new String[300000][];
    for (int i = 0; i < pairs.length; i++) pairs[i] = pair(6 + rand(9));
    long t = System.nanoTime();
    double sum = 0;
    for (String[] p : pairs) sum += jaroWinkler(p[0], p[1]);
    boolean ok = Math.abs(sum - 260943.0356007) < 0.000001
        && Math.abs(jaroWinkler("MARTHA", "MARHTA") - 0.961111111) < 0.000000001;
    Bench.report("jaro_winkler", t, ok);
  }
}
