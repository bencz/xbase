// bench/generator.prg in Java: an iterator of 3,000,000 numbers and a second one that takes the even ones from the
// first (each step a call, as the xBase enumerators).
import java.util.Iterator;
import java.util.NoSuchElementException;

public class Generator {
  static Iterator<Long> range(long a, long b) {
    return new Iterator<>() {
      long next = a;
      public boolean hasNext() { return next <= b; }
      public Long next() { return next++; }
    };
  }

  static Iterator<Long> evens(Iterator<Long> e) {
    return new Iterator<>() {
      Long pending;
      public boolean hasNext() {
        while (pending == null && e.hasNext()) {
          long x = e.next();
          if (x % 2 == 0) pending = x;
        }
        return pending != null;
      }
      public Long next() {
        if (!hasNext()) throw new NoSuchElementException();
        Long x = pending;
        pending = null;
        return x;
      }
    };
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), s = 0;
    for (Iterator<Long> it = evens(range(1, 3000000)); it.hasNext();) s += it.next();
    Bench.report("generator", t, s == 2250001500000L);
  }
}
