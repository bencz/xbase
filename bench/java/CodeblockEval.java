// bench/codeblock_eval.prg in Java: 10,000,000 calls of each of three lambdas kept in an array (one ignoring its
// arguments, one of two arguments, one that reads a captured local), the results summed.
public class CodeblockEval {
  interface Block {
    long eval(long x, long y);
  }

  static Block[] blocks(long base) {
    return new Block[] {(x, y) -> 7, (x, y) -> x * 2 + y, (x, y) -> x + base};
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    Block[] b = blocks(3);
    for (long i = 1; i <= 10000000; i++) n += b[0].eval(0, 0) + b[1].eval(i % 100, 1) + b[2].eval(i % 10, 0);
    Bench.report("codeblock_eval", t, n == 1145000000);
  }
}
