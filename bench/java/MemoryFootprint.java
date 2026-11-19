// bench/memory_footprint.prg in Java: the heap in use after full collections (System.gc(), then Runtime's total less
// free memory): an array of 1,000,000 longs, 200,000 objects of a class of three fields, a HashMap of 300,000 string
// keys, 600,000 records { number, 11-character string } as Object arrays, each made by a method and measured before
// it returns. Each must stay within the .prg's budget of heap bytes per element, and once they are all dropped the
// heap must come back near where it started. The time is that of making them and of the collections.
import java.util.HashMap;

public class MemoryFootprint {
  static final Runtime RT = Runtime.getRuntime();

  static long used() {
    System.gc();
    return RT.totalMemory() - RT.freeMemory();
  }

  // Whether a structure of `len` elements, expected to hold n, is within nBytes heap bytes per element.
  static boolean within(int len, int n, long nBytes, long heap) {
    long nUsed = used() - heap;
    return len == n && nUsed <= n * nBytes;
  }

  static boolean integers(long heap) {
    long[] a = new long[1000000];
    for (int i = 0; i < a.length; i++) a[i] = (i + 1) * 3L;
    return within(a.length, 1000000, 24, heap) && a[0] == 3;
  }

  static class TPoint {
    long x = 0, y = 0;
    String cTag = "point";
    TPoint init(long px, long py) {
      x = px;
      y = py;
      return this;
    }
  }

  static boolean objects(long heap) {
    TPoint[] a = new TPoint[200000];
    for (int i = 0; i < a.length; i++) a[i] = new TPoint().init(i + 1, -(i + 1));
    return within(a.length, 200000, 128, heap) && a[0].x == 1;
  }

  static boolean keys(long heap) {
    HashMap<String, Long> h = new HashMap<>();
    for (long i = 1; i <= 300000; i++) h.put("k" + String.format("%7d", i), i);
    return within(h.size(), 300000, 224, heap) && h.containsKey("k      1");
  }

  static boolean records(long heap) {
    Object[][] a = new Object[600000][];
    for (int i = 0; i < a.length; i++) a[i] = new Object[] {(long) (i + 1), "item" + String.format("%7d", i + 1)};
    return within(a.length, 600000, 224, heap) && a[0][1].equals("item      1");
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), heap = used();
    boolean ok = integers(heap) && objects(heap) && keys(heap) && records(heap);
    ok = ok && used() <= heap + 16 * 1024 * 1024;
    Bench.report("memory_footprint", t, ok);
  }
}
