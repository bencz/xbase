// bench/closure_capture.prg in Java: 500,000 counters made by a function, each a lambda holding its own copy of a
// local (in a one-element array, as a lambda captures no variable it changes) and a parameter, each called 4 times;
// then 2,000,000 calls of lambdas that share one captured local and change it.
import java.util.function.LongSupplier;
import java.util.function.LongUnaryOperator;

public class ClosureCapture {
  static LongUnaryOperator counter(long step) {
    long[] count = {0};
    return x -> count[0] += step + x;
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    LongUnaryOperator[] a = new LongUnaryOperator[500000];
    for (int i = 0; i < a.length; i++) a[i] = counter((i + 1) % 10);
    for (long j = 1; j <= 4; j++)
      for (LongUnaryOperator b : a) n += b.applyAsLong(j);
    long[] shared = {0};
    LongUnaryOperator inc = x -> shared[0] += x;
    LongSupplier read = () -> shared[0] % 1000;
    for (long i = 1; i <= 2000000; i++) {
      inc.applyAsLong(i % 7);
      n += read.getAsLong();
    }
    Bench.report("closure_capture", t, n == 1031500997 && shared[0] == 5999997);
  }
}
