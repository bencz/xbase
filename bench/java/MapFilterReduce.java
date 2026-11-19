// bench/map_filter_reduce.prg in Java: map, filter and reduce written out over arrays with lambdas (the filter's
// result in an ArrayList, as AAdd grows the .prg's): 1,000,000 numbers mapped, filtered and reduced to a sum; 8 rounds
// with other lambdas.
import java.util.ArrayList;
import java.util.List;
import java.util.function.LongBinaryOperator;
import java.util.function.LongPredicate;
import java.util.function.LongUnaryOperator;

public class MapFilterReduce {
  static long[] map(long[] a, LongUnaryOperator b) {
    long[] out = new long[a.length];
    for (int i = 0; i < a.length; i++) out[i] = b.applyAsLong(a[i]);
    return out;
  }

  static List<Long> filter(long[] a, LongPredicate b) {
    List<Long> out = new ArrayList<>();
    for (long x : a)
      if (b.test(x)) out.add(x);
    return out;
  }

  static long reduce(List<Long> a, LongBinaryOperator b, long acc) {
    for (long x : a) acc = b.applyAsLong(acc, x);
    return acc;
  }

  public static void main(String[] args) {
    long[] a = new long[1000000];
    for (int i = 0; i < a.length; i++) a[i] = (i + 1) % 1000;
    long t = System.nanoTime(), n = 0;
    for (int k = 1; k <= 8; k++) {
      long r = k;
      n += reduce(filter(map(a, x -> x * r + 1), x -> x % 3 == 0), (acc, x) -> acc + x, 0);
    }
    Bench.report("map_filter_reduce", t, n == 4492503000L);
  }
}
