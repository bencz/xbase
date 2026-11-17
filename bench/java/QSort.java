// bench/qsort.prg in Java: recursive quicksort over 200,000 Park-Miller numbers, 5 times.
public class QSort {
  static void qs(long[] a, int lo, int hi) {
    while (lo < hi) {
      long pivot = a[(lo + hi) / 2];
      int i = lo, j = hi;
      while (i <= j) {
        while (a[i] < pivot) i++;
        while (a[j] > pivot) j--;
        if (i <= j) {
          long tmp = a[i];
          a[i] = a[j];
          a[j] = tmp;
          i++;
          j--;
        }
      }
      if (j - lo < hi - i) {
        qs(a, lo, j);
        lo = i;
      } else {
        qs(a, i, hi);
        hi = j;
      }
    }
  }

  static boolean sortRandom() {
    int n = 200000;
    long[] a = new long[n + 1]; // 1-based, as the xBase array
    long x = 12345, sum = 0;
    boolean sorted = true;
    for (int i = 1; i <= n; i++) {
      x = (x * 16807) % 2147483647;
      a[i] = x % 1000000;
    }
    qs(a, 1, n);
    for (int i = 2; i <= n; i++)
      if (a[i - 1] > a[i]) sorted = false;
    for (int i = 1; i <= n; i += 1000) sum += a[i];
    return sorted && a[1] == 6 && a[100000] == 500294 && a[n] == 999998 && sum == 99556483;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    boolean ok = true;
    for (int rep = 1; rep <= 5; rep++) ok = ok && sortRandom();
    Bench.report("qsort", t, ok);
  }
}
