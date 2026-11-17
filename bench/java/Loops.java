// bench/loops.prg in Java: the sum of the first 500,000,000 odd numbers.
public class Loops {
  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0, limit = 500000000;
    for (long i = 1; i <= limit; i++) n = n + i * 2 - 1;
    Bench.report("loops", t, n == 250000000000000000L);
  }
}
