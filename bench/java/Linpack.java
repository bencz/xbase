// bench/linpack.prg in Java: LINPACK: a dense 300 x 300 system (the original's matrix generator) solved by LU
// factorization with partial pivoting (DGEFA, DGESL, through DAXPY, IDAMAX and DSCAL on one column-major array), 5
// times; the check is the solution, all ones, and the residual of A x - b. Indexes are 1-based as in the .prg (element
// 0 unused).
public class Linpack {
  // The original's test matrix: entries from a congruential sequence in [-2, 2), b the sums of the rows.
  static void matGen(double[] a, double[] b, int n) {
    long init = 1325;
    for (int j = 1; j <= n; j++)
      for (int i = 1; i <= n; i++) {
        init = (3125 * init) % 65536;
        a[(j - 1) * n + i] = (init - 32768.0) / 16384.0;
      }
    java.util.Arrays.fill(b, 1, n + 1, 0.0);
    for (int j = 1; j <= n; j++)
      for (int i = 1; i <= n; i++) b[i] += a[(j - 1) * n + i];
  }

  // LU factorization of the column-major n x n matrix a in place, the pivot rows in ipvt.
  static void dgefa(double[] a, int n, int[] ipvt) {
    for (int k = 1; k <= n - 1; k++) {
      int col = (k - 1) * n;
      int l = idamax(n - k + 1, a, col + k) + k - 1;
      ipvt[k] = l;
      if (a[col + l] != 0.0) {
        double t;
        if (l != k) {
          t = a[col + l];
          a[col + l] = a[col + k];
          a[col + k] = t;
        }
        t = -1.0 / a[col + k];
        dscal(n - k, t, a, col + k + 1);
        for (int j = k + 1; j <= n; j++) {
          t = a[(j - 1) * n + l];
          if (l != k) {
            a[(j - 1) * n + l] = a[(j - 1) * n + k];
            a[(j - 1) * n + k] = t;
          }
          daxpy(n - k, t, a, col + k + 1, a, (j - 1) * n + k + 1);
        }
      }
    }
    ipvt[n] = n;
  }

  // Solves a x = b with the factors of dgefa; b becomes x.
  static void dgesl(double[] a, int n, int[] ipvt, double[] b) {
    for (int k = 1; k <= n - 1; k++) {
      int l = ipvt[k];
      double t = b[l];
      if (l != k) {
        b[l] = b[k];
        b[k] = t;
      }
      daxpy(n - k, t, a, (k - 1) * n + k + 1, b, k + 1);
    }
    for (int k = n; k >= 1; k--) {
      b[k] = b[k] / a[(k - 1) * n + k];
      double t = -b[k];
      daxpy(k - 1, t, a, (k - 1) * n + 1, b, 1);
    }
  }

  // dy[ y .. y + n - 1 ] += da * dx[ x .. x + n - 1 ]
  static void daxpy(int n, double da, double[] dx, int x, double[] dy, int y) {
    if (n > 0 && da != 0.0)
      for (int i = 0; i <= n - 1; i++) dy[y + i] += da * dx[x + i];
  }

  // dx[ x .. x + n - 1 ] *= da
  static void dscal(int n, double da, double[] dx, int x) {
    for (int i = x; i <= x + n - 1; i++) dx[i] = da * dx[i];
  }

  // The 1-based position of the largest magnitude in dx[ x .. x + n - 1 ] (the first of equal ones).
  static int idamax(int n, double[] dx, int x) {
    int imax = 1;
    double dmax = Math.abs(dx[x]);
    for (int i = 2; i <= n; i++) {
      double v = Math.abs(dx[x + i - 1]);
      if (v > dmax) {
        imax = i;
        dmax = v;
      }
    }
    return imax;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    int n = 300;
    boolean ok = true;
    for (int rep = 1; rep <= 5; rep++) {
      double[] a = new double[n * n + 1], b = new double[n + 1];
      int[] ipvt = new int[n + 1];
      matGen(a, b, n);
      double[] x = b.clone();
      dgefa(a, n, ipvt);
      dgesl(a, n, ipvt, x);
      // the factorization overwrote a: a new copy gives the residual
      matGen(a, b, n);
      double maxErr = 0.0, resid = 0.0;
      for (int i = 1; i <= n; i++) {
        maxErr = Math.max(maxErr, Math.abs(x[i] - 1.0));
        double s = -b[i];
        for (int j = 1; j <= n; j++) s += a[(j - 1) * n + i] * x[j];
        resid = Math.max(resid, Math.abs(s));
      }
      ok = ok && maxErr < 0.000000001 && resid < 0.000000001;
    }
    Bench.report("linpack", t, ok);
  }
}
