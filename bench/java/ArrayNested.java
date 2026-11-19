// bench/array_nested.prg in Java: a 100 x 100 x 100 cube of nested arrays filled, then 10 passes that change the
// cells from their neighbors along the last two dimensions and sum them.
public class ArrayNested {
  public static void main(String[] args) {
    long t = System.nanoTime(), sum = 0;
    long[][][] a = new long[100][100][100];
    for (int i = 1; i <= 100; i++)
      for (int j = 1; j <= 100; j++)
        for (int k = 1; k <= 100; k++) a[i - 1][j - 1][k - 1] = (i * 31 + j * 17 + k * 7) % 101;
    for (int p = 1; p <= 10; p++) {
      for (int i = 0; i < 100; i++) {
        long[][] plane = a[i];
        for (int j = 1; j < 100; j++) {
          long[] row = plane[j];
          for (int k = 1; k < 100; k++) row[k] = (row[k] + plane[j - 1][k] + row[k - 1]) % 1000;
        }
      }
      for (int i = 0; i < 100; i++)
        for (int j = 0; j < 100; j++)
          for (int k = 0; k < 100; k++) sum += a[i][j][k];
    }
    Bench.report("array_nested", t, sum == 4905893027L);
  }
}
