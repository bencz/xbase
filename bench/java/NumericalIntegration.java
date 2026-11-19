// bench/numerical_integration.prg in Java: Simpson's rule over 1,000,000 intervals for four integrals whose values
// are known (4 / (1 + x^2) and Sqrt(x) on [0, 1], Exp(-x^2) on [0, 1], 1 / x on [1, 2]), each function a lambda, 3
// times.
import java.util.function.DoubleUnaryOperator;

public class NumericalIntegration {
  static double simpson(DoubleUnaryOperator f, double a, double b, long n) {
    double h = (b - a) / n, s = f.applyAsDouble(a) + f.applyAsDouble(b);
    for (long i = 1; i <= n - 1; i++) s = s + ((i & 1) == 1 ? 4.0 : 2.0) * f.applyAsDouble(a + i * h);
    return s * h / 3.0;
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), n = 1000000;
    boolean ok = true;
    for (int rep = 1; rep <= 3; rep++) {
      double r1 = simpson(x -> 4.0 / (1.0 + x * x), 0.0, 1.0, n);
      double r2 = simpson(x -> Math.exp(-(x * x)), 0.0, 1.0, n);
      double r3 = simpson(x -> 1.0 / x, 1.0, 2.0, n);
      double r4 = simpson(x -> Math.sqrt(x), 0.0, 1.0, n);
      ok = ok && Math.abs(r1 - 3.141592653589793) < 0.000000000001 && Math.abs(r2 - 0.746824132812427) < 0.000000000001
          && Math.abs(r3 - 0.6931471805599453) < 0.000000000001 && Math.abs(r4 - 2.0 / 3.0) < 0.000000001;
    }
    Bench.report("numerical_integration", t, ok);
  }
}
