// bench/binary_trees.prg in Java: binary trees of two-element arrays made and walked, depths 4 to 14 (a stretch tree
// of depth 15, one of depth 14 that lives throughout, 2^(14 - d + 4) trees of each even depth d).
public class BinaryTrees {
  static Object[] make(int d) {
    if (d == 0) return new Object[2];
    return new Object[] {make(d - 1), make(d - 1)};
  }

  static long check(Object[] node) {
    if (node[0] == null) return 1;
    return 1 + check((Object[]) node[0]) + check((Object[]) node[1]);
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), total = 0;
    int maxDepth = 14;
    total += check(make(maxDepth + 1));
    Object[] aLong = make(maxDepth);
    for (int d = 4; d <= maxDepth; d += 2) {
      int iters = 1 << (maxDepth - d + 4);
      for (int i = 1; i <= iters; i++) total += check(make(d));
    }
    total += check(aLong);
    Bench.report("binary_trees", t, total == 3222190);
  }
}
