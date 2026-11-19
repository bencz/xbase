// bench/inheritance_depth.prg in Java: a line of 8 classes, each extending the one before: 4,000,000 rounds, on an
// object of the last made by name through reflection (as the .prg's macro), of a method each class overrides and that
// calls super's down to the first (8 calls), a method only the first class has, and fields declared by the first, a
// middle and the last class (read, and the first one assigned).
public class InheritanceDepth {
  static class TL0 {
    long v0 = 0;
    long depth() { return 1; }
    long base() { return v0 + 100; }
  }

  static class TL1 extends TL0 {
    long v1 = 1;
    @Override long depth() { return super.depth() + 2; }
  }

  static class TL2 extends TL1 {
    long v2 = 2;
    @Override long depth() { return super.depth() + 3; }
  }

  static class TL3 extends TL2 {
    long v3 = 3;
    @Override long depth() { return super.depth() + 4; }
  }

  static class TL4 extends TL3 {
    long v4 = 4;
    @Override long depth() { return super.depth() + 5; }
  }

  static class TL5 extends TL4 {
    long v5 = 5;
    @Override long depth() { return super.depth() + 6; }
  }

  static class TL6 extends TL5 {
    long v6 = 6;
    @Override long depth() { return super.depth() + 7; }
  }

  static class TL7 extends TL6 {
    long v7 = 7;
    @Override long depth() { return super.depth() + 8; }
  }

  public static void main(String[] args) throws ReflectiveOperationException {
    long t = System.nanoTime(), n = 0;
    TL7 o = (TL7) Class.forName("InheritanceDepth$TL7").getDeclaredConstructor().newInstance();
    for (long i = 1; i <= 4000000; i++) {
      n += o.depth() + o.base() + o.v0 + o.v4 + o.v7;
      o.v0 = i % 3;
    }
    // o.depth() is 8 + 7 + ... + 1 = 36, o.base() is o.v0 + 100, o.v4 + o.v7 is 11; o.v0 is (i - 1) % 3, 0 at first
    Bench.report("inheritance_depth", t, n == 4000000L * (36 + 100 + 11) + 2 * 3999999L);
  }
}
