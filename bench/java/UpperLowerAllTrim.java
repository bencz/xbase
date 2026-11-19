// bench/upper_lower_alltrim.prg in Java: toUpperCase, toLowerCase, strip, stripLeading and stripTrailing of 1,000
// mixed-case strings padded with blanks on both sides, 1,500,000 times.
import java.util.Locale;

public class UpperLowerAllTrim {
  static long seed = 8;
  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  public static void main(String[] args) {
    String[] a = new String[1000];
    String chars = "aBcDeFgHiJkLmNoPqRsTuVwXyZ 0123456789";
    for (int i = 0; i < a.length; i++) {
      StringBuilder w = new StringBuilder();
      for (long j = 1; j <= 5 + rand(30); j++) // the limit drawn again at every test, as xBase's FOR does
        w.append(chars.charAt((int) rand(37)));
      a[i] = " ".repeat((int) rand(6)) + "x" + w + "Q" + " ".repeat((int) rand(6));
    }
    long t = System.nanoTime(), n = 0;
    for (int i = 1; i <= 1500000; i++) {
      String x = a[i % 1000], w = x.strip(), l = w.toLowerCase(Locale.ROOT);
      n += w.length() + w.toUpperCase(Locale.ROOT).charAt(0) + l.charAt(l.length() - 1) + x.stripTrailing().length()
          - x.stripLeading().length();
    }
    Bench.report("upper_lower_alltrim", t, n == 320344500);
  }
}
