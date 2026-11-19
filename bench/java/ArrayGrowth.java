// bench/array_growth.prg in Java: many lists growing and shrinking: 300,000 lists grown with add() to 1 to 64
// elements, enlarged to twice that, cut to half, an element inserted in front and the last dropped (AIns), the first
// removed and an empty one appended (ADel); one in 100 kept to the end. An empty element is null.
import java.util.ArrayList;

public class ArrayGrowth {
  static long seed = 3;

  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  // ASize(): grows with empty elements or cuts the tail.
  static void resize(ArrayList<Long> a, int n) {
    while (a.size() < n) a.add(null);
    while (a.size() > n) a.remove(a.size() - 1);
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    ArrayList<ArrayList<Long>> kept = new ArrayList<>();
    for (long i = 1; i <= 300000; i++) {
      ArrayList<Long> a = new ArrayList<>();
      int size = (int) rand(64) + 1;
      for (long j = 1; j <= size; j++) a.add(j * 2);
      resize(a, size * 2);
      a.set(size * 2 - 1, i % 1000);
      n += a.get(size * 2 - 1);
      resize(a, (size + 1) / 2);
      a.add(0, null);
      a.remove(a.size() - 1);
      a.set(0, (long) size);
      a.remove(0);
      a.add(null);
      a.set(a.size() - 1, i % 7);
      n += a.size() + a.get(a.size() - 1) + (a.size() > 1 ? a.get(0) : 0);
      if (i % 100 == 0) kept.add(a);
    }
    for (ArrayList<Long> a : kept) n += a.get(0);
    Bench.report("array_growth", t, n == 156283982 && kept.size() == 3000);
  }
}
