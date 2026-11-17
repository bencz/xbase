// bench/asort.prg in Java: the library's sort, 300,000 numbers ascending and 100,000 with a descending comparison
// (boxed, as a comparison needs objects), 5 times.
import java.util.Arrays;

public class ASort {
  static boolean sortBoth() {
    long[] a = new long[300000];
    Long[] b = new Long[100000];
    long x = 777, sum = 0;
    for (int i = 0; i < a.length; i++) {
      x = (x * 16807) % 2147483647;
      a[i] = x % 1000000;
    }
    x = 4242;
    for (int i = 0; i < b.length; i++) {
      x = (x * 16807) % 2147483647;
      b[i] = x % 1000000;
    }
    Arrays.sort(a);
    Arrays.sort(b, (p, q) -> Long.compare(q, p));
    for (int i = 0; i < a.length; i += 1000) sum += a[i];
    return a[0] == 8 && a[149999] == 500673 && a[299999] == 999998 && sum == 149589926 && b[0] == 999994
        && b[49999] == 501282 && b[99999] == 8;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    boolean ok = true;
    for (int rep = 1; rep <= 5; rep++) ok = ok && sortBoth();
    Bench.report("asort", t, ok);
  }
}
