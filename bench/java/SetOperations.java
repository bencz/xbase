// bench/set_operations.prg in Java: sets as HashMaps (each key its own value), two sets of 200,000 draws each from
// 0 .. 399,999, their union, intersection, difference and symmetric difference built as new maps; 2 rounds with new
// members.
import java.util.HashMap;

public class SetOperations {
  static long seed = 777;
  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), check = 0;
    HashMap<Long, Long> ha = null, hb = null, union = null, inter = null, diff = null, sym = null;
    for (int r = 1; r <= 2; r++) {
      ha = new HashMap<>();
      hb = new HashMap<>();
      for (int i = 1; i <= 200000; i++) {
        long k = rand(400000);
        ha.put(k, k);
        k = rand(400000);
        hb.put(k, k);
      }
      union = new HashMap<>();
      inter = new HashMap<>();
      diff = new HashMap<>();
      sym = new HashMap<>();
      for (Long k : ha.values()) {
        union.put(k, k);
        if (hb.containsKey(k)) {
          inter.put(k, k);
        } else {
          diff.put(k, k);
          sym.put(k, k);
        }
      }
      for (Long k : hb.values()) {
        union.put(k, k);
        if (!ha.containsKey(k)) sym.put(k, k);
      }
      check += ha.size() + hb.size() * 2L + union.size() * 3L + inter.size() * 5L + diff.size() * 7L
          + sym.size() * 11L;
    }
    boolean ok = check == 8609885 && union.size() == 252664 && union.size() == inter.size() + sym.size()
        && ha.size() == inter.size() + diff.size();
    Bench.report("set_operations", t, ok);
  }
}
