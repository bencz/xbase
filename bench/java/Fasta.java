// bench/fasta.prg in Java: the fasta benchmark for n = 1,500,000: the ALU sequence repeated over 2n characters, then
// 3n IUB codes and 5n Homo sapiens bases picked from cumulative probabilities with the benchmark's LCG (seed 42), in
// lines of 60, appended to one StringBuilder of 15 MB.
public class Fasta {
  static final long IM = 139968, IA = 3877, IC = 29573;
  static final int LINE = 60;

  // n characters of the ALU sequence, cyclically, in lines of 60.
  static String repeatSeq(int n) {
    String cAlu = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTC"
        + "AGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCG"
        + "TGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGG"
        + "AGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";
    int k = cAlu.length(), nPos = 0;
    StringBuilder out = new StringBuilder();
    for (int nDone = 0; nDone < n; nDone += LINE) {
      int m = Math.min(n - nDone, LINE);
      for (int i = 0; i < m; i++) {
        out.append(cAlu.charAt(nPos));
        if (++nPos >= k) nPos = 0;
      }
      out.append('\n');
    }
    return out.toString();
  }

  // n symbols drawn with the LCG in state[0] (the sum of their indexes, from 0, added to state[1]), in lines of 60.
  static String randomSeq(long[] state, String cSym, double[] aProb, int n) {
    int nSym = aProb.length;
    double[] aCum = new double[nSym];
    double nAcc = 0;
    for (int i = 0; i < nSym; i++) {
      nAcc += aProb[i];
      aCum[i] = nAcc;
    }
    long seed = state[0], nPicks = 0;
    StringBuilder out = new StringBuilder();
    for (int nDone = 0; nDone < n; nDone += LINE) {
      int m = Math.min(n - nDone, LINE);
      for (int i = 0; i < m; i++) {
        seed = (seed * IA + IC) % IM;
        double r = (double) seed / IM;
        int j = 0;
        while (j < nSym - 1 && r >= aCum[j]) j++;
        nPicks += j;
        out.append(cSym.charAt(j));
      }
      out.append('\n');
    }
    state[0] = seed;
    state[1] += nPicks;
    return out.toString();
  }

  public static void main(String[] args) {
    long t = System.nanoTime();
    int n = 1500000;
    long[] state = {42, 0};
    StringBuilder out = new StringBuilder();
    out.append(">ONE Homo sapiens alu\n");
    out.append(repeatSeq(2 * n));
    out.append(">TWO IUB ambiguity codes\n");
    out.append(randomSeq(state, "acgtBDHKMNRSVWY",
        new double[] {0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02},
        3 * n));
    out.append(">THREE Homo sapiens frequency\n");
    out.append(randomSeq(state, "acgt", new double[] {0.3029549426680, 0.1979883004921, 0.1975473066391,
        0.3015094502008}, 5 * n));
    String c = out.toString();
    int nTwo = c.indexOf(">TWO") + 1;
    boolean ok = c.length() == 15250077 && state[0] == 28650 && state[1] == 25406718 && nTwo == 3050023
        && c.substring(nTwo - 22, nTwo - 1).equals("TGGGCGACAGAGCGAGACTC\n")
        && c.endsWith("acacatatagttttatacaa\n");
    Bench.report("fasta", t, ok);
  }
}
