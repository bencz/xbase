// bench/gc.prg in Java: 40,000 slots holding strings of 16 to 2,000 characters and small records of them, replaced at
// random 3,000,000 times, while 20,000 records that live to the end are kept beside them.
import java.util.ArrayList;

public class Gc {
  static long seed = 7;
  static long rand(long n) {
    seed = (seed * 1103515245L + 12345L) % 2147483648L;
    return (seed / 65536) % n;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    Object[] slots = new Object[40000];
    ArrayList<Object[]> kept = new ArrayList<>();
    for (long i = 1; i <= 3000000; i++) {
      int n = (int) rand(slots.length);
      Object x;
      if (rand(4) == 0)
        x = new Object[] {String.valueOf((char) (65 + i % 26)).repeat((int) (16 + rand(200))), i,
                          " ".repeat((int) rand(100))};
      else
        x = String.valueOf((char) (97 + i % 26)).repeat((int) (16 + rand(2000)));
      slots[n] = x;
      if (i % 150 == 0) kept.add(new Object[] {i, String.format("%10d", i), "k".repeat(64)});
    }
    long check = 0;
    for (Object x : slots) {
      if (x instanceof String s) check += s.length();
      else if (x instanceof Object[] a) check += ((String) a[0]).length() + ((String) a[2]).length();
    }
    Bench.report("gc", t, kept.size() == 20000 && check > 0);
  }
}
