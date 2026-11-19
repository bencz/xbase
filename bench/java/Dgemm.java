// bench/dgemm.prg in Java: DGEMM: C := alpha A B + beta C on 250 x 250 matrices of doubles in flat row-major arrays
// (loops in i, k, j order), the inputs from a linear congruential generator, 3 times; the check is the sum and the
// trace of C. Indexes are 1-based as in the .prg (element 0 unused).
public class Dgemm {
  public static void main(String[] args) {
    long t = System.nanoTime(), seed = 7;
    int n = 250;
    double alpha = 1.5, beta = 0.5, sum = 0.0, trace = 0.0;
    double[] a = new double[n * n + 1], b = new double[n * n + 1], c = new double[n * n + 1];
    for (int i = 1; i <= n * n; i++) {
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      a[i] = seed / 1073741824.0 - 1.0;
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      b[i] = seed / 1073741824.0 - 1.0;
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      c[i] = seed / 1073741824.0 - 1.0;
    }
    for (int rep = 1; rep <= 3; rep++) {
      for (int i = 0; i <= n - 1; i++) {
        int ci = i * n;
        for (int j = ci + 1; j <= ci + n; j++) c[j] = beta * c[j];
        for (int k = 0; k <= n - 1; k++) {
          double aik = alpha * a[ci + k + 1];
          int bk = k * n - ci;
          for (int j = ci + 1; j <= ci + n; j++) c[j] += aik * b[bk + j];
        }
      }
    }
    for (int i = 1; i <= n; i++) {
      for (int j = 1; j <= n; j++) sum += c[(i - 1) * n + j];
      trace += c[(i - 1) * n + i];
    }
    boolean ok = Math.abs(sum + 145.62810535074) < 0.000001 && Math.abs(trace - 342.80906789597) < 0.000001;
    Bench.report("dgemm", t, ok);
  }
}
