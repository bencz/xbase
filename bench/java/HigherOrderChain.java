// bench/higher_order_chain.prg in Java: chains of streams over an array of 500,000 numbers, each step fed by the one
// before: filter, map, skip, takeWhile and sum; map with the position, filter, limit and count; distinct by a key and
// count; 5 rounds.
import java.util.HashSet;
import java.util.stream.IntStream;
import java.util.stream.LongStream;

public class HigherOrderChain {
  public static void main(String[] args) {
    long[] a = new long[500000];
    for (int i = 0; i < a.length; i++) a[i] = ((i + 1) * 37L) % 1000;
    long t = System.nanoTime(), n = 0;
    for (long r = 1; r <= 5; r++) {
      long m = r, k = 100 * r;
      n += LongStream.of(a).filter(x -> x % 3 != 0).map(x -> x * m).skip(10).takeWhile(x -> x < 4900).sum();
      n += IntStream.range(0, a.length).mapToLong(i -> a[i] + (i + 1) % 7).filter(x -> x % 2 == 0).limit(50000 + r)
          .count();
      HashSet<Long> seen = new HashSet<>();
      n += LongStream.of(a).filter(x -> seen.add(x % k)).count();
    }
    Bench.report("higher_order_chain", t, n == 1663633050);
  }
}
