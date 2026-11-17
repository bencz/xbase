// bench/matmul.prg in Java: the product of two 100 x 100 integer matrices, 20 times.
public class MatMul {
  public static void main(String[] args) {
    long t = System.nanoTime();
    int n = 100;
    long[][] a = new long[n][n], b = new long[n][n], c = new long[n][n];
    for (int i = 0; i < n; i++)
      for (int j = 0; j < n; j++) {
        a[i][j] = (i + j) % 7;
        b[i][j] = (i * j) % 5;
      }
    long total = 0;
    for (int rep = 1; rep <= 20; rep++)
      for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
          long s = 0;
          for (int k = 0; k < n; k++) s += a[i][k] * b[k][j];
          c[i][j] = s;
          total += s;
        }
    Bench.report("matmul", t, total == 20L * 4798200);
  }
}
