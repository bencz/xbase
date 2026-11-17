// bench/fib.prg in Java: recursive calls, Fib( 37 ).
public class Fib {
  static long fib(long n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

  public static void main(String[] args) {
    long t = System.nanoTime();
    Bench.report("fib", t, fib(37) == 24157817);
  }
}
