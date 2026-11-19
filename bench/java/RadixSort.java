// bench/radix_sort.prg in Java: least-significant-digit radix sort, four passes of one byte each, of 300,000 31-bit
// integers from a linear congruential generator, between two arrays, 10 times with different seeds; the order and a
// weighted checksum are checked.
import java.util.Arrays;

public class RadixSort {
  public static void main(String[] args) {
    long t = System.nanoTime(), sum = 0;
    int n = 300000;
    long[] a = new long[n], b = new long[n], cnt = new long[256];
    boolean ok = true;
    for (int rep = 1; rep <= 10; rep++) {
      long seed = rep + 30;
      for (int i = 0; i < n; i++) {
        seed = (seed * 1103515245L + 12345) & 0x7FFFFFFF;
        a[i] = seed;
      }
      for (int d = 0; d <= 3; d++) {
        int shift = d * 8;
        Arrays.fill(cnt, 0);
        for (int i = 0; i < n; i++) cnt[(int) ((a[i] >> shift) & 0xFF)]++;
        long pos = 1;
        for (int k = 0; k < 256; k++) {
          long x = cnt[k];
          cnt[k] = pos;
          pos += x;
        }
        for (int i = 0; i < n; i++) {
          long x = a[i];
          int k = (int) ((x >> shift) & 0xFF);
          b[(int) cnt[k] - 1] = x;
          cnt[k]++;
        }
        long[] tmp = a;
        a = b;
        b = tmp;
      }
      for (int i = 1; i < n; i++)
        if (a[i - 1] > a[i]) ok = false;
      for (int i = 1; i <= n; i++) sum += a[i - 1] * ((i & 7) + 1);
    }
    Bench.report("radix_sort", t, ok && sum == 14501540638159929L);
  }
}
