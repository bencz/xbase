// bench/k_nucleotide.prg in Java: HashMaps keyed by strings: the Homo sapiens section of fasta for n = 100,000
// (500,000 bases, upper case, made with the benchmark's LCG before the clock starts), every k-mer of it counted in a
// HashMap for k = 1, 2, 3, 4, 6, 12 and 18 (a substring per position as the key), the 1- and 2-mers sorted by count
// with the library's sort, and the counts of five given sequences.
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

public class KNucleotide {
  static final long IM = 139968, IA = 3877, IC = 29573;

  // Every k-mer of s with its count.
  static HashMap<String, Long> frames(String s, int k) {
    HashMap<String, Long> h = new HashMap<>();
    for (int i = 0; i <= s.length() - k; i++) {
      String key = s.substring(i, i + k);
      Long c = h.get(key);
      h.put(key, c == null ? 1 : c + 1);
    }
    return h;
  }

  static long countOf(String s, String seq) {
    return frames(s, seq.length()).getOrDefault(seq, 0L);
  }

  // The k-mers by count, then by name: "A 151655 T 150452 ...".
  static String sorted(String s, int k) {
    ArrayList<Map.Entry<String, Long>> pairs = new ArrayList<>(frames(s, k).entrySet());
    pairs.sort((x, y) -> !x.getValue().equals(y.getValue()) ? Long.compare(y.getValue(), x.getValue())
        : x.getKey().compareTo(y.getKey()));
    StringBuilder out = new StringBuilder();
    for (Map.Entry<String, Long> p : pairs) {
      if (out.length() > 0) out.append(' ');
      out.append(p.getKey()).append(' ').append(p.getValue());
    }
    return out.toString();
  }

  public static void main(String[] args) {
    long n = 100000, seed = 42;
    double[] prob = {0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008}, cum = new double[4];
    double acc = 0;
    for (int i = 0; i < 4; i++) {
      acc += prob[i];
      cum[i] = acc;
    }
    for (long i = 1; i <= 3 * n; i++) seed = (seed * IA + IC) % IM;
    StringBuilder sb = new StringBuilder();
    for (long i = 1; i <= 5 * n; i++) {
      seed = (seed * IA + IC) % IM;
      double r = (double) seed / IM;
      int j = 0;
      while (j < 3 && r >= cum[j]) j++;
      sb.append("ACGT".charAt(j));
    }
    String s = sb.toString();
    long t = System.nanoTime();
    boolean ok = sorted(s, 1).equals("A 151655 T 150452 C 99156 G 98737")
        && sorted(s, 2).equals("AA 45992 TA 45599 AT 45574 TT 45299 CA 30129 AC 30096 AG 29993 GA 29935 CT 29857 "
            + "TC 29832 GT 29722 TG 29721 GC 19617 CC 19611 CG 19559 GG 19463")
        && countOf(s, "GGT") == 5871 && countOf(s, "GGTA") == 1769 && countOf(s, "GGTATT") == 185
        && countOf(s, "GGTATTTTAATT") == 4 && countOf(s, "GGTATTTTAATTTATAGT") == 4;
    Bench.report("k_nucleotide", t, ok);
  }
}
