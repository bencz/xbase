// bench/short_lived_objects.prg in Java: 2,000,000 small vectors made with new, each added to a running sum by a
// method that returns a new vector, so three objects are made and as many dropped per step (the JIT's escape analysis
// may remove some of them).
public class ShortLivedObjects {
  static final class TVec {
    final long x, y;
    TVec(long x, long y) {
      this.x = x;
      this.y = y;
    }
    TVec add(TVec o) { return new TVec(x + o.x, y + o.y); }
    TVec scaled(long k) { return new TVec(x * k, y * k); }
    long dot(TVec o) { return x * o.x + y * o.y; }
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    TVec s = new TVec(0, 0);
    for (long i = 1; i <= 2000000; i++) {
      TVec v = new TVec(i % 10, i % 7 - 3);
      s = s.add(v);
      n += v.dot(s.scaled(0)) + v.x;
    }
    Bench.report("short_lived_objects", t, s.x == 9000000 && s.y == -3 && n == 9000000);
  }
}
