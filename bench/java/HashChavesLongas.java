// bench/hash_chaves_longas.prg in Java: a HashMap with long string keys, 300,000 keys of 82 to 131 characters that
// share a 66-character prefix, inserted, then looked up through other strings of the same text, and 300,000 missing
// keys of the same shape looked up.
import java.util.HashMap;

public class HashChavesLongas {
  static long seed = 99;
  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  public static void main(String[] args) {
    String prefix = "https://example.com/catalog/products/category/electronics/item?id=";
    String[] keys = new String[300000], same = new String[300000], miss = new String[300000];
    for (int i = 1; i <= keys.length; i++) {
      long n = rand(1000000);
      String x = "x".repeat(i % 50);
      keys[i - 1] = prefix + String.format("%07d", i) + "&v=" + String.format("%06d", n) + x;
      same[i - 1] = prefix + String.format("%07d", i) + "&v=" + String.format("%06d", n) + x;
      miss[i - 1] = prefix + String.format("%07d", i) + "&w=" + String.format("%06d", n) + x;
    }
    long t = System.nanoTime(), sum = 0, missing = 0;
    HashMap<String, Long> h = new HashMap<>();
    for (int i = 0; i < keys.length; i++) h.put(keys[i], i + 1L);
    for (int i = same.length - 1; i >= 0; i--) sum += h.get(same[i]);
    for (String k : miss)
      if (!h.containsKey(k)) missing++;
    Bench.report("hash_chaves_longas", t, h.size() == 300000 && sum == 45000150000L && missing == 300000);
  }
}
