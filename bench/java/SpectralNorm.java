// bench/spectral_norm.prg in Java: the spectral norm of A(i,j) = 1 / ((i+j)(i+j+1)/2 + i + 1) cut to 1,200 x 1,200,
// 10 rounds of the power method on A transposed times A, each element of A from a function call.
public class SpectralNorm {
  static double evalA(long i, long j) { return 1.0 / ((i + j) * (i + j + 1) / 2 + i + 1); }

  static void mulAv(int n, double[] u, double[] v) {
    for (int i = 0; i < n; i++) {
      double s = 0.0;
      for (int j = 0; j < n; j++) s += evalA(i, j) * u[j];
      v[i] = s;
    }
  }

  static void mulAtv(int n, double[] u, double[] v) {
    for (int i = 0; i < n; i++) {
      double s = 0.0;
      for (int j = 0; j < n; j++) s += evalA(j, i) * u[j];
      v[i] = s;
    }
  }

  static void mulAtAv(int n, double[] u, double[] v, double[] w) {
    mulAv(n, u, w);
    mulAtv(n, w, v);
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    int n = 1200;
    double[] u = new double[n], v = new double[n], w = new double[n];
    java.util.Arrays.fill(u, 1.0);
    for (int i = 0; i < 10; i++) {
      mulAtAv(n, u, v, w);
      mulAtAv(n, v, u, w);
    }
    double vbv = 0.0, vv = 0.0;
    for (int i = 0; i < n; i++) {
      vbv += u[i] * v[i];
      vv += v[i] * v[i];
    }
    double r = Math.sqrt(vbv / vv);
    Bench.report("spectral_norm", t, Math.abs(r - 1.2742241501) < 0.000000001);
  }
}
