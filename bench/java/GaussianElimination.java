// bench/gaussian_elimination.prg in Java: a 300 x 300 system of random doubles (a fixed seed) whose solution is all
// ones, solved by Gaussian elimination with partial pivoting (rows swapped as references) and back substitution, 6
// times.
public class GaussianElimination {
  public static void main(String[] args) {
    long t = System.nanoTime();
    int n = 300;
    boolean ok = true;
    for (int rep = 1; rep <= 6; rep++) {
      long seed = 42;
      double[][] a = new double[n][n + 1];
      for (int i = 0; i < n; i++) {
        double s = 0.0;
        for (int j = 0; j < n; j++) {
          seed = (seed * 1103515245L + 12345) & 0x7FFFFFFF;
          a[i][j] = seed / 2147483648.0 - 0.5;
          s = s + a[i][j];
        }
        a[i][n] = s;
      }
      for (int k = 0; k < n; k++) {
        int p = k;
        for (int i = k + 1; i < n; i++)
          if (Math.abs(a[i][k]) > Math.abs(a[p][k])) p = i;
        double[] piv = a[p];
        a[p] = a[k];
        a[k] = piv;
        for (int i = k + 1; i < n; i++) {
          double[] row = a[i];
          double f = row[k] / piv[k];
          for (int j = k; j <= n; j++) row[j] = row[j] - f * piv[j];
        }
      }
      double[] x = new double[n];
      double err = 0.0;
      for (int i = n - 1; i >= 0; i--) {
        double[] row = a[i];
        double s = row[n];
        for (int j = i + 1; j < n; j++) s = s - row[j] * x[j];
        x[i] = s / row[i];
        err = Math.max(err, Math.abs(x[i] - 1.0));
      }
      ok = ok && err < 0.000000001;
    }
    Bench.report("gaussian_elimination", t, ok);
  }
}
