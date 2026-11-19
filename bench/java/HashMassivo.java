// bench/hash_massivo.prg in Java: 1,000,000 numeric keys from a fixed-seed generator counted in one HashMap (about
// 632,000 distinct), then 1,500,000 lookups of keys drawn from twice the range.
import java.util.HashMap;

public class HashMassivo {
  static long seed = 2024;

  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), hits = 0, sum = 0;
    HashMap<Long, Long> h = new HashMap<>();
    for (int i = 1; i <= 1000000; i++) {
      long k = rand(1000000);
      if (h.containsKey(k)) h.put(k, h.get(k) + 1);
      else h.put(k, 1L);
    }
    for (int i = 1; i <= 1500000; i++) {
      long k = rand(2000000);
      if (h.containsKey(k)) {
        hits++;
        sum += h.get(k);
      }
    }
    Bench.report("hash_massivo", t, h.size() == 631967 && hits == 473472 && sum == 748930);
  }
}
