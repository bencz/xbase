// bench/counting_sort.prg in Java: counting sort of 1,000,000 keys in 0 .. 65535 (the high bits of a linear
// congruential generator), stable through prefix sums into an output array, 6 times with different seeds; the order
// and a weighted checksum are checked.
import java.util.Arrays;

public class CountingSort {
  public static void main(String[] args) {
    long t = System.nanoTime(), sum = 0;
    int n = 1000000, m = 65536;
    long[] a = new long[n], b = new long[n], cnt = new long[m];
    boolean ok = true;
    for (int rep = 1; rep <= 6; rep++) {
      long seed = rep + 20;
      for (int i = 0; i < n; i++) {
        seed = seed * 16807 % 2147483647;
        a[i] = seed / 32768;
      }
      Arrays.fill(cnt, 0);
      for (int i = 0; i < n; i++) cnt[(int) a[i]]++;
      long pos = 1;
      for (int k = 0; k < m; k++) {
        long c = cnt[k];
        cnt[k] = pos;
        pos += c;
      }
      for (int i = 0; i < n; i++) {
        int k = (int) a[i];
        b[(int) cnt[k] - 1] = a[i];
        cnt[k]++;
      }
      for (int i = 1; i < n; i++)
        if (b[i - 1] > b[i]) ok = false;
      for (int i = 1; i <= n; i++) sum += b[i - 1] * (i % 8 + 1);
    }
    Bench.report("counting_sort", t, ok && sum == 884633960242L);
  }
}
