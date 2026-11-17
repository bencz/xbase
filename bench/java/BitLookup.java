// bench/bitlookup.prg in Java: the bitwise AND from a table of the ANDs of two nibbles, IOR and XOR written out as the
// .prg's macros expand them (7 calls of and() a pair; the JIT may inline them), for every pair a, b in 0 .. 999.
public class BitLookup {
  static final int[] TABLE = new int[256];
  static {
    for (int a = 0; a < 16; a++)
      for (int b = 0; b < 16; b++) TABLE[a * 16 + b] = a & b;
  }

  static int and(int a, int b) {
    int r = 0;
    for (int i = 0; i < 16; i += 4) {
      r = r / 16 + TABLE[(a % 16) * 16 + (b % 16)] * 4096;
      a /= 16;
      b /= 16;
    }
    return r;
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    boolean ok = true;
    outer:
    for (int a = 0; a < 1000; a++)
      for (int b = 0; b < 1000; b++)
        if (and(a, b) != (a & b)
            || (a + b - 2 * and(a, b)) + and(a, b) - 2 * and(a + b - 2 * and(a, b), and(a, b)) != (a | b)
            || a + b - 2 * and(a, b) != (a ^ b)) {
          ok = false;
          break outer;
        }
    Bench.report("bitlookup", t, ok);
  }
}
