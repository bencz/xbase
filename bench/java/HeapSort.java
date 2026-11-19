// bench/heap_sort.prg in Java: heap sort in place (a binary max-heap built bottom-up, then sifted down) of 300,000
// integers from a linear congruential generator, 3 times with different seeds; the order and a weighted checksum are
// checked.
public class HeapSort {
  // Moves a[ i ] down the heap a[ 1 .. n ] until neither child is larger.
  static void siftDown(long[] a, int i, int n) {
    long x = a[i];
    int c;
    while ((c = i * 2) <= n) {
      if (c < n && a[c + 1] > a[c]) c++;
      if (a[c] <= x) break;
      a[i] = a[c];
      i = c;
    }
    a[i] = x;
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), sum = 0;
    int n = 300000;
    long[] a = new long[n + 1];
    boolean ok = true;
    for (long rep = 1; rep <= 3; rep++) {
      long seed = rep + 10;
      for (int i = 1; i <= n; i++) {
        seed = seed * 16807 % 2147483647;
        a[i] = seed;
      }
      for (int i = n / 2; i >= 1; i--) siftDown(a, i, n);
      for (int i = n; i >= 2; i--) {
        long x = a[1];
        a[1] = a[i];
        a[i] = x;
        siftDown(a, 1, i - 1);
      }
      for (int i = 2; i <= n; i++)
        if (a[i - 1] > a[i]) ok = false;
      for (int i = 1; i <= n; i++) sum += a[i] * (i % 8 + 1);
    }
    Bench.report("heap_sort", t, ok && sum == 4351668891394344L);
  }
}
