// bench/method_call_overhead.prg in Java: 5,000,000 rounds of calls with no argument, one and three arguments, a
// short method and a method that calls another through this; on an object made with new, then on one made through
// reflection (the .prg's macro; the JIT still sees its class).
public class MethodCallOverhead {
  static class TCalc {
    long base = 1;
    long zero() { return base; }
    long one(long x) { return x; }
    long three(long x, long y, long z) { return x + y + z - 5; }
    long twice(long x) { return x + x; }
    long chain(long x) { return one(x) + one(x) + 1; }
  }

  public static void main(String[] args) throws ReflectiveOperationException {
    long t = System.nanoTime(), n = 0;
    TCalc o = new TCalc();
    TCalc u = (TCalc) Class.forName("MethodCallOverhead$TCalc").getDeclaredConstructor().newInstance();
    for (long i = 1; i <= 5000000; i++) {
      n += o.zero() + o.one(i) + o.three(i, 2, 3) + o.twice(i) + o.chain(i);
      n += u.zero() + u.one(i) + u.three(i, 2, 3) + u.twice(i) + u.chain(i);
    }
    // each round adds 2 * (6 * i + 2)
    Bench.report("method_call_overhead", t, n == 150000050000000L);
  }
}
