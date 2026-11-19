// bench/ackermann.prg in Java: deep recursion, Ackermann's function A(3, 9) (11 million calls, 4093 levels deep), 3
// times.
public class Ackermann {
  static long ack(long m, long n) {
    if (m == 0) return n + 1;
    if (n == 0) return ack(m - 1, 1);
    return ack(m - 1, ack(m, n - 1));
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    boolean ok = true;
    for (int rep = 1; rep <= 3; rep++) ok = ok && ack(3, 9) == 4093;
    Bench.report("ackermann", t, ok);
  }
}
