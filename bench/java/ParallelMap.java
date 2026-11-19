// bench/parallel_map.prg in Java: 1,000,000 numbers (1 to 1,000,000, from a fixed seed) mapped to their Collatz step
// counts by 64 tasks on the common pool (CompletableFuture), each mapping its slice into an array of its own; the
// slices joined in order (all of them awaited) and the result checked: its sum, its maximum and where it is first, a
// running hash of the whole.
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;

public class ParallelMap {
  static long[] mapSlice(long[] in, int from, int count) {
    long[] a = new long[count];
    for (int i = 0; i < count; i++) a[i] = steps(in[from + i]);
    return a;
  }

  static long steps(long x) {
    long s = 0;
    while (x != 1) {
      x = x % 2 == 0 ? x / 2 : 3 * x + 1;
      s++;
    }
    return s;
  }

  public static void main(String[] args) {
    int n = 1000000;
    long[] in = new long[n], out = new long[n];
    long x = 777;
    for (int i = 0; i < n; i++) {
      x = x * 16807 % 2147483647;
      in[i] = x % 1000000 + 1;
    }
    long t = System.nanoTime(), nSum = 0, nMax = 0, nAt = 0, nChk = 0;
    int nChunk = n / 64, nPos = 0;
    List<CompletableFuture<long[]>> tasks = new ArrayList<>();
    for (int i = 0; i < n; i += nChunk) {
      int from = i;
      tasks.add(CompletableFuture.supplyAsync(() -> mapSlice(in, from, nChunk)));
    }
    CompletableFuture.allOf(tasks.toArray(new CompletableFuture<?>[0])).join();
    for (CompletableFuture<long[]> f : tasks) {
      long[] a = f.join();
      System.arraycopy(a, 0, out, nPos, a.length);
      nPos += a.length;
    }
    for (int i = 1; i <= n; i++) {
      x = out[i - 1];
      nSum += x;
      if (x > nMax) {
        nMax = x;
        nAt = i;
      }
      nChk = (nChk * 31 + x) % 1000000007;
    }
    Bench.report("parallel_map", t,
        nPos == n && nSum == 131359344 && nMax == 524 && nAt == 237731 && nChk == 68222219);
  }
}
