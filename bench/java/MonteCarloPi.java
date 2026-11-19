// bench/monte_carlo_pi.prg in Java: 50,000,000 points of the unit square from a 31-bit linear congruential generator,
// the count of those inside the quarter circle.
public class MonteCarloPi {
  public static void main(String[] args) {
    long t = System.nanoTime(), n = 50000000, seed = 42, inside = 0;
    for (long i = 1; i <= n; ++i) {
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      double x = seed / 2147483648.0;
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      double y = seed / 2147483648.0;
      if (x * x + y * y <= 1.0) ++inside;
    }
    double est = 4.0 * inside / n;
    Bench.report("monte_carlo_pi", t, inside == 39267523 && Math.abs(est - 3.14159) < 0.01);
  }
}
