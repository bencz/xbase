// bench/mixed.prg in Java: integers and doubles in one expression, double to integer, comparisons across both.
public class Mixed {
  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    double s = 0;
    for (long i = 1; i <= 20000000; i++) {
      double d = i + 3.14;
      s += (long) (d * 4) % 7;
      if (d > i + 3) n++;
      s += i / 4.0;
    }
    Bench.report("mixed", t, s == 50000062499998.0 && n == 20000000);
  }
}
