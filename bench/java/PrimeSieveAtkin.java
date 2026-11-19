// bench/prime_sieve_atkin.prg in Java: sieve of Atkin, the primes up to 2,000,000 from the three quadratic forms
// 4x^2+y^2, 3x^2+y^2 and 3x^2-y^2 taken modulo 12 (toggling an array of booleans), then the multiples of the squares
// of primes struck out; 12 times.
import java.util.Arrays;

public class PrimeSieveAtkin {
  public static void main(String[] args) {
    long t = System.nanoTime();
    int limit = 2000000;
    boolean[] a = new boolean[limit + 1];
    boolean ok = true;
    for (int rep = 1; rep <= 12; rep++) {
      Arrays.fill(a, false);
      for (long x = 1; x * x <= limit; x++) {
        for (long y = 1; y * y <= limit; y++) {
          long n = 4 * x * x + y * y;
          if (n <= limit) {
            long r = n % 12;
            if (r == 1 || r == 5) a[(int) n] = !a[(int) n];
          }
          n = 3 * x * x + y * y;
          if (n <= limit && n % 12 == 7) a[(int) n] = !a[(int) n];
          if (x > y) {
            n = 3 * x * x - y * y;
            if (n <= limit && n % 12 == 11) a[(int) n] = !a[(int) n];
          }
        }
      }
      for (long i = 5; i * i <= limit; i++) {
        if (a[(int) i]) {
          long k = i * i;
          for (long j = k; j <= limit; j += k) a[(int) j] = false;
        }
      }
      long count = 2;
      for (int i = 5; i <= limit; i++)
        if (a[i]) count++;
      ok = ok && count == 148933;
    }
    Bench.report("prime_sieve_atkin", t, ok);
  }
}
