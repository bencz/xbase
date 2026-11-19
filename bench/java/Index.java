// bench/index.prg in Java: an ordered index over a table: 200,000 records with random keys (held in memory, not in a
// file), an order built on them (a TreeSet of key and record number), 100,000 random seeks, a walk through the order,
// 20,000 records appended with the order kept up to date.
import java.util.ArrayList;
import java.util.TreeSet;

public class Index {
  record Key(String key, int rec) implements Comparable<Key> {
    @Override public int compareTo(Key o) {
      int c = key.compareTo(o.key);
      return c != 0 ? c : Integer.compare(rec, o.rec);
    }
  }

  static long seed = 7;

  static long rand(long n) {
    seed = (seed * 1103515245 + 12345) % 2147483648L;
    return seed / 65536 % n;
  }

  static String zero10(long n) {
    return String.format("%010d", n);
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), hits = 0, count = 0;
    int recs = 200000;
    ArrayList<String> k = new ArrayList<>();
    ArrayList<Long> n = new ArrayList<>();
    for (int i = 1; i <= recs; i++) {
      k.add(zero10(rand(100000000)));
      n.add((long) i);
    }
    TreeSet<Key> order = new TreeSet<>();
    for (int i = 0; i < k.size(); i++) order.add(new Key(k.get(i), i + 1));
    for (int i = 1; i <= 100000; i++) {
      String s = zero10(rand(100000000));
      Key at = order.ceiling(new Key(s, 0));
      if (at != null && at.key.equals(s)) hits++;
    }
    boolean ok = true;
    String prev = "";
    for (Key e : order) {
      String key = k.get(e.rec - 1);
      ok = ok && key.compareTo(prev) >= 0;
      prev = key;
      count++;
    }
    ok = ok && count == recs;
    for (int i = 1; i <= 20000; i++) {
      k.add(zero10(rand(100000000)));
      n.add((long) recs + i);
      order.add(new Key(k.get(k.size() - 1), k.size()));
    }
    count = 0;
    for (Key e : order) {
      if (e.rec > 0) count++;
    }
    ok = ok && count == recs + 20000 && hits >= 0 && n.size() == k.size();
    Bench.report("index", t, ok);
  }
}
