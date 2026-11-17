// bench/sieve.prg in Java: the primes up to 2,000,000, 10 times.
public class Sieve {
  public static void main(String[] args) {
    long t = System.nanoTime();
    int n = 2000000;
    boolean[] a = new boolean[n + 1];
    boolean ok = true;
    for (int rep = 1; rep <= 10; rep++) {
      int count = 0;
      for (int i = 2; i <= n; i++) a[i] = true;
      a[1] = false;
      for (int i = 2; i * i <= n; i++)
        if (a[i])
          for (int j = i * i; j <= n; j += i) a[j] = false;
      for (int i = 1; i <= n; i++)
        if (a[i]) count++;
      ok = ok && count == 148933;
    }
    Bench.report("sieve", t, ok);
  }
}
