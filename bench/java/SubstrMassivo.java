// bench/substr_massivo.prg in Java: 6,000,000 substrings of 1 to 20 characters cut at spread positions of a
// 1,000,000-character text, their first and last characters read.
public class SubstrMassivo {
  static long seed = 5;

  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  public static void main(String[] args) {
    StringBuilder b = new StringBuilder();
    for (int i = 1; i <= 1000000; i++) b.append((char) (33 + rand(90)));
    String c = b.toString();
    long t = System.nanoTime(), n = 0;
    for (long i = 1; i <= 6000000; i++) {
      int from = (int) ((i * 7919) % 999970);
      String s = c.substring(from, from + (int) (i % 20) + 1);
      n += s.charAt(0) + s.charAt(s.length() - 1) + s.length();
    }
    Bench.report("substr_massivo", t, n == 994529563);
  }
}
