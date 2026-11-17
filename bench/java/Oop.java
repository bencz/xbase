// bench/oop.prg in Java: method calls (one calling its parent's through super), field reads and assignments in a loop
// of 3,000,000.
public class Oop {
  static class TBase {
    long total;
    void add(long x) { total += x; }
  }

  static class TCounter extends TBase {
    long n;
    @Override void add(long x) { super.add(x); }
    long result() { return total + n; }
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    TCounter o = new TCounter();
    for (long i = 1; i <= 3000000; i++) {
      o.add(i % 7);
      o.n = o.n + 1;
    }
    Bench.report("oop", t, o.result() == 11999997);
  }
}
