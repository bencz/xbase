// bench/levenshtein.prg in Java: the Levenshtein distance of 10,000 pairs of strings of 30 to 60 letters (the second
// one the first with random edits), by dynamic programming over two rows, the letters read with charAt().
public class Levenshtein {
  static long seed = 1001;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static long distance(String s, String u) {
    int m = s.length(), n = u.length();
    long[] prev = new long[n + 1], cur = new long[n + 1], swap;
    for (int j = 0; j <= n; j++) prev[j] = j;
    for (int i = 1; i <= m; i++) {
      cur[0] = i;
      char c = s.charAt(i - 1);
      for (int j = 1; j <= n; j++)
        cur[j] = Math.min(Math.min(prev[j], cur[j - 1]) + 1, prev[j - 1] + (c == u.charAt(j - 1) ? 0 : 1));
      swap = prev;
      prev = cur;
      cur = swap;
    }
    return prev[n];
  }

  static String[] pair(long len) {
    StringBuilder s = new StringBuilder(), u = new StringBuilder();
    for (long i = 1; i <= len; i++) s.append((char) (97 + rand(4)));
    for (int i = 0; i < len; i++) {
      long k = rand(10);
      if (k == 0) u.append((char) (97 + rand(4)));
      else if (k == 1) u.append(s.charAt(i)).append((char) (97 + rand(4)));
      else if (k != 2) u.append(s.charAt(i));
    }
    return new String[] {s.toString(), u.toString()};
  }

  public static void main(String[] args) {
    String[][] pairs = new String[10000][];
    for (int i = 0; i < pairs.length; i++) pairs[i] = pair(30 + rand(31));
    long t = System.nanoTime(), n = 0;
    for (String[] p : pairs) n += distance(p[0], p[1]);
    Bench.report("levenshtein", t, n == 108309 && distance("kitten", "sitting") == 3);
  }
}
