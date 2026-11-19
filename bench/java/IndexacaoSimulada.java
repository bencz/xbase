// bench/indexacao_simulada.prg in Java: an index kept in memory, 150,000 entries (an 8-digit key and the record
// number, one string each) from a fixed seed, sorted with the list's sort, 100,000 seeks by binary search (a key
// compared over its length, as < does with SET EXACT OFF), 3,000 entries inserted in place, and the order checked.
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class IndexacaoSimulada {
  static long seed = 99991;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  // The first position whose entry is not below k, compared over k's length (a.size() when there is none).
  static int lowerBound(List<String> a, String k) {
    int lo = 0, hi = a.size();
    while (lo < hi) {
      int m = (lo + hi) / 2;
      String e = a.get(m);
      if (e.substring(0, Math.min(e.length(), k.length())).compareTo(k) < 0) lo = m + 1;
      else hi = m;
    }
    return lo;
  }

  public static void main(String[] args) {
    int recs = 150000;
    List<String> a = new ArrayList<>(recs);
    for (int i = 1; i <= recs; i++) a.add(String.format(Locale.ROOT, "C%08d%06d", rand(10000000), i));
    long t = System.nanoTime(), hits = 0, sum = 0, chk = 0;
    boolean ok = true;
    a.sort(null);
    for (int i = 1; i <= 100000; i++) {
      String k = String.format(Locale.ROOT, "C%08d", rand(10000000));
      int p = lowerBound(a, k);
      if (p < a.size() && a.get(p).startsWith(k)) {
        hits++;
        sum += Long.parseLong(a.get(p).substring(9));
      }
    }
    for (int i = 1; i <= 3000; i++) {
      String k = String.format(Locale.ROOT, "C%08d%06d", rand(10000000), recs + i);
      a.add(lowerBound(a, k), k);
    }
    for (int i = 1; i < a.size(); i++) ok = ok && a.get(i - 1).compareTo(a.get(i)) < 0;
    for (int i = 0; i < a.size(); i += 997) chk = (chk * 31 + Long.parseLong(a.get(i).substring(9))) % 1000000007;
    ok = ok && hits == 1426 && sum == 107771969 && chk == 409559053 && a.get(0).equals("C00000040027737")
        && a.get(a.size() - 1).equals("C09999947069110");
    Bench.report("indexacao_simulada", t, ok);
  }
}
