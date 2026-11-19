// bench/property_access.prg in Java: 3,000,000 rounds of field reads and assignments (with =, += and ++), a computed
// getter and a setter, on one object; the .prg's second block (WITH OBJECT) is the same accesses through the object.
public class PropertyAccess {
  static class TAccount {
    long nBalance, nCount, nLimit;
    String cLast = "";

    long doubled() { return nBalance * 2 - nCount - 2; }

    void limit(long x) { nLimit = x * 10; }
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    TAccount o = new TAccount();
    for (long i = 1; i <= 3000000; i++) {
      o.nBalance += 2;
      o.nCount++;
      o.cLast = "d";
      n += o.nBalance - o.nCount + o.doubled();
      o.limit(i % 10);
      o.nBalance -= 1;
      n += o.nLimit + o.cLast.length();
    }
    Bench.report("property_access", t, n == 4500001500000L + 10 * 13500000L + 2 * 3000000L && o.nBalance == 3000000);
  }
}
