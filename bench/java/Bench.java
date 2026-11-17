// Shared by the Java benchmarks: the result line the xBase benchmarks print, `<name> <milliseconds> ok|WRONG`.
final class Bench {
  private Bench() {}

  static void report(String name, long startNanos, boolean ok) {
    System.out.println(name + " " + (System.nanoTime() - startNanos) / 1000000 + " " + (ok ? "ok" : "WRONG"));
  }
}
