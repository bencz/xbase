// bench/hash_resize.prg in Java: a HashMap that grows and shrinks, 20 rounds of 100,000 keys added to an empty map and
// removed again, the even ones first, then the odd ones.
import java.util.HashMap;

public class HashResize {
  public static void main(String[] args) {
    long t = System.nanoTime(), sum = 0;
    boolean ok = true;
    for (long r = 1; r <= 20; r++) {
      HashMap<Long, Long> h = new HashMap<>();
      for (long i = 1; i <= 100000; i++) h.put(i * 7 + r, i);
      ok = ok && h.size() == 100000;
      for (long i = 2; i <= 100000; i += 2) {
        sum += h.get(i * 7 + r);
        h.remove(i * 7 + r);
      }
      ok = ok && h.size() == 50000;
      for (long i = 1; i <= 100000; i += 2) {
        sum += h.get(i * 7 + r);
        h.remove(i * 7 + r);
      }
      ok = ok && h.isEmpty();
    }
    Bench.report("hash_resize", t, ok && sum == 100001000000L);
  }
}
