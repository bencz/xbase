// bench/object_churn.prg in Java: 50,000 slots holding objects of three classes that point to other slots' objects
// (cycles included); 700,000 times a slot's object is replaced by a new one, then the graph is walked.
import java.util.Locale;

public class ObjectChurn {
  static long seed = 9;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static class TLeaf {
    long value;
    TLeaf init(long n) {
      value = n;
      return this;
    }
    long weight() { return value; }
  }

  static class TPair extends TLeaf {
    TLeaf left, right;
    TPair init(long n, TLeaf l, TLeaf r) {
      value = n;
      left = l;
      right = r;
      return this;
    }
    @Override long weight() { return value + left.value + right.value; }
  }

  static class TList extends TLeaf {
    Object[] items;
    TList init(long n, TLeaf o) {
      value = n;
      // the .prg's { o, n, Str( n ) }: n is a remainder, a number with 2 decimals, so Str() makes 13 columns
      items = new Object[] {o, n, String.format(Locale.ROOT, "%13.2f", (double) n)};
      return this;
    }
    @Override long weight() { return value + ((TLeaf) items[0]).value + ((String) items[2]).length(); }
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    TLeaf[] slots = new TLeaf[50000];
    for (int i = 0; i < slots.length; i++) slots[i] = new TLeaf().init(i + 1);
    for (long i = 1; i <= 700000; i++) {
      int k = (int) rand(slots.length);
      long kind = rand(3);
      TLeaf o;
      if (kind == 0) {
        o = new TLeaf().init(i % 100);
      } else if (kind == 1) {
        TLeaf l = slots[(int) rand(slots.length)];
        o = new TPair().init(i % 100, l, slots[(int) rand(slots.length)]);
      } else {
        o = new TList().init(i % 100, slots[(int) rand(slots.length)]);
      }
      slots[k] = o;
    }
    for (TLeaf o : slots) n += o.weight();
    Bench.report("object_churn", t, n == 5162688);
  }
}
