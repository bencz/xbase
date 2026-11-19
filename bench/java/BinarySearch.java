// bench/binary_search.prg in Java: binary search written out, 1,000,000 keys from a fixed-seed generator looked for in
// a sorted array of 1,000,000 numbers (every third number), about a third of them found.
public class BinarySearch {
  static long seed = 4711;
  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  public static void main(String[] args) {
    long[] a = new long[1000000];
    for (int i = 1; i <= a.length; i++) a[i - 1] = i * 3L;
    long t = System.nanoTime(), found = 0, posSum = 0;
    for (int i = 1; i <= 1000000; i++) {
      long k = rand(3000010);
      long lo = 1, hi = a.length;
      while (lo <= hi) {
        long mid = (lo + hi) / 2;
        if (a[(int) mid - 1] < k) {
          lo = mid + 1;
        } else if (a[(int) mid - 1] > k) {
          hi = mid - 1;
        } else {
          found++;
          posSum += mid;
          break;
        }
      }
    }
    Bench.report("binary_search", t, found == 333181 && posSum == 166512579113L);
  }
}
