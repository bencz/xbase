// bench/merge_sort.prg in Java: recursive top-down merge sort of 300,000 integers from a linear congruential
// generator, through one work array, 3 times with different seeds; the order and a weighted checksum are checked.
public class MergeSort {
  static void mergeSort(long[] a, long[] w, int lo, int hi) {
    if (hi - lo < 1) return;
    int mid = (lo + hi) / 2;
    mergeSort(a, w, lo, mid);
    mergeSort(a, w, mid + 1, hi);
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
      if (a[j] < a[i]) w[k++] = a[j++];
      else w[k++] = a[i++];
    }
    while (i <= mid) w[k++] = a[i++];
    while (j <= hi) w[k++] = a[j++];
    for (k = lo; k <= hi; k++) a[k] = w[k];
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), sum = 0;
    int n = 300000;
    long[] a = new long[n + 1], w = new long[n + 1];
    boolean ok = true;
    for (int rep = 1; rep <= 3; rep++) {
      long seed = rep;
      for (int i = 1; i <= n; i++) {
        seed = seed * 16807 % 2147483647;
        a[i] = seed;
      }
      mergeSort(a, w, 1, n);
      for (int i = 2; i <= n; i++)
        if (a[i - 1] > a[i]) ok = false;
      for (int i = 1; i <= n; i++) sum += a[i] * (i % 8 + 1);
    }
    Bench.report("merge_sort", t, ok && sum == 4349496350073402L);
  }
}
