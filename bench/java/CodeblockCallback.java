// bench/codeblock_callback.prg in Java: functions that take a lambda, Simpson's rule over 3 functions (1,000,000
// intervals each) and an insertion sort of 3,000 numbers that compares through a lambda, 10 times.
import java.util.function.DoubleUnaryOperator;

public class CodeblockCallback {
  static long seed = 1;

  interface Less {
    boolean test(long x, long y);
  }

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static double simpson(DoubleUnaryOperator f, double a, double b, int n) {
    double h = (b - a) / n, sum = f.applyAsDouble(a) + f.applyAsDouble(b);
    for (int i = 1; i <= n - 1; i++) sum += f.applyAsDouble(a + i * h) * (i % 2 == 1 ? 4 : 2);
    return sum * h / 3;
  }

  static void insertionSort(long[] a, Less less) {
    for (int i = 1; i < a.length; i++) {
      long x = a[i];
      int j = i - 1;
      while (j >= 0 && less.test(x, a[j])) {
        a[j + 1] = a[j];
        j--;
      }
      a[j + 1] = x;
    }
  }

  public static void main(String[] args) {
    long[] data = new long[3000];
    for (int i = 0; i < data.length; i++) data[i] = rand(100000);
    long t = System.nanoTime(), check = 0;
    double sum = 0;
    sum += simpson(x -> x * x, 0, 3, 1000000);
    sum += simpson(x -> 1 / (1 + x * x), 0, 1, 1000000);
    sum += simpson(x -> x * x * x - 2 * x, -1, 2, 1000000);
    for (int r = 1; r <= 10; r++) {
      long[] a = data.clone();
      insertionSort(a, r % 2 == 0 ? (x, y) -> x < y : (x, y) -> x > y);
      check += a[0] + a[1499] * 2 + a[2999] * 3;
    }
    Bench.report("codeblock_callback", t, Math.abs(sum - 10.535398163397) < 0.000000001 && check == 2991620);
  }
}
