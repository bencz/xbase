// bench/array_preallocate.prg in Java: 25 arrays of 2,000,000 numbers made at their size, filled by index, then
// summed by index (array_append builds the same arrays by appending).
public class ArrayPreallocate {
  public static void main(String[] args) {
    long t = System.nanoTime(), sum = 0;
    int n = 2000000;
    long[] a = null;
    for (int r = 1; r <= 25; r++) {
      a = new long[n];
      for (int i = 0; i < n; i++) a[i] = i + 1 + r;
      for (int i = 0; i < n; i++) sum += a[i];
    }
    Bench.report("array_preallocate", t, sum == 50000675000000L && a.length == n);
  }
}
