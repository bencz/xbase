// bench/strings.prg in Java: Left, Upper, Len and At on 20,000,000 short strings.
public class Strings {
  public static void main(String[] args) {
    long t = System.nanoTime(), total = 0;
    String src = "abcdefghij";
    for (int i = 1; i <= 20000000; i++) {
      String w = src.substring(0, i % 10 + 1).toUpperCase(java.util.Locale.ROOT);
      total += w.length() + w.indexOf('D') + 1;
    }
    Bench.report("strings", t, total == 166000000);
  }
}
