// bench/array_of_hashes.prg in Java: records as HashMaps, 120,000 maps of four fields made from a fixed-seed
// generator, totals per group read by field name, a count of the records that pass a test, and the list sorted by a
// field with a comparator.
import java.util.ArrayList;
import java.util.HashMap;

public class ArrayOfHashes {
  static long seed = 31337;
  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  static long num(HashMap<String, Object> r, String k) { return (Long) r.get(k); }

  public static void main(String[] args) {
    long t = System.nanoTime(), pass = 0, sum = 0;
    ArrayList<HashMap<String, Object>> a = new ArrayList<>();
    long[] total = new long[50];
    for (long i = 1; i <= 120000; i++) {
      HashMap<String, Object> r = new HashMap<>();
      r.put("id", i);
      r.put("name", "N" + String.format("%05d", rand(100000)));
      r.put("score", rand(1000));
      r.put("group", rand(50) + 1);
      a.add(r);
    }
    for (HashMap<String, Object> r : a) {
      total[(int) num(r, "group") - 1] += num(r, "score");
      if (num(r, "score") >= 500 && ((String) r.get("name")).startsWith("N1")) pass++;
    }
    a.sort((x, y) -> Long.compare(num(x, "score"), num(y, "score")));
    for (int i = 0; i < a.size(); i += 1000) sum += num(a.get(i), "score");
    boolean ok = total[0] == 1239678 && total[49] == 1170281 && pass == 5950 && sum == 59429
        && num(a.get(0), "score") == 0 && num(a.get(119999), "score") == 999;
    Bench.report("array_of_hashes", t, ok);
  }
}
