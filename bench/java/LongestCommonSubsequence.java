// bench/longest_common_subsequence.prg in Java: the length of the longest common subsequence of 600 pairs of 200 to
// 300 letters from a 4-letter alphabet (the second related to the first), by dynamic programming over two rows, the
// letters split into arrays first.
public class LongestCommonSubsequence {
  static long seed = 31;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static long lcs(String s, String u) {
    int m = s.length(), n = u.length();
    char[] aS = s.toCharArray(), aU = u.toCharArray();
    long[] prev = new long[n + 1], cur = new long[n + 1], swap;
    for (int i = 0; i < m; i++) {
      char c = aS[i];
      for (int j = 0; j < n; j++) {
        if (c == aU[j]) cur[j + 1] = prev[j] + 1;
        else cur[j + 1] = Math.max(prev[j + 1], cur[j]);
      }
      swap = prev;
      prev = cur;
      cur = swap;
    }
    return prev[n];
  }

  static String[] pair(long len) {
    StringBuilder s = new StringBuilder(), u = new StringBuilder();
    for (long i = 1; i <= len; i++) s.append((char) (65 + rand(4)));
    for (int i = 0; i < len; i++) {
      if (rand(4) != 0) u.append(s.charAt(i));
      if (rand(5) == 0) u.append((char) (65 + rand(4)));
    }
    return new String[] {s.toString(), u.toString()};
  }

  public static void main(String[] args) {
    String[][] pairs = new String[600][];
    for (int i = 0; i < pairs.length; i++) pairs[i] = pair(200 + rand(101));
    long t = System.nanoTime(), n = 0;
    for (String[] p : pairs) n += lcs(p[0], p[1]);
    Bench.report("longest_common_subsequence", t, n == 118188 && lcs("ABCBDAB", "BDCABA") == 4);
  }
}
