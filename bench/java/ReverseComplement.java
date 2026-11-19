// bench/reverse_complement.prg in Java: the output of fasta for n = 2,500,000 (25 MB, made with the benchmark's LCG
// before the clock starts) read section by section a character at a time, each sequence joined, reverse-complemented
// through the IUB table (an array indexed by the character code) and written again in lines of 60 after its header.
public class ReverseComplement {
  static final long IM = 139968, IA = 3877, IC = 29573;
  static final int LINE = 60;

  // The sequence reverse-complemented, in lines of 60.
  static String revComp(CharSequence seq, char[] comp) {
    StringBuilder out = new StringBuilder();
    int nCol = 0;
    for (int i = seq.length() - 1; i >= 0; i--) {
      out.append(comp[seq.charAt(i)]);
      if (++nCol == LINE) {
        out.append('\n');
        nCol = 0;
      }
    }
    if (nCol > 0) out.append('\n');
    return out.toString();
  }

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

  static String randomSeq(long[] seedBox, String cSym, double[] aProb, int n) {
    int nSym = aProb.length;
    double[] aCum = new double[nSym];
    double nAcc = 0;
    for (int i = 0; i < nSym; i++) {
      nAcc += aProb[i];
      aCum[i] = nAcc;
    }
    long seed = seedBox[0];
    StringBuilder out = new StringBuilder();
    for (int nDone = 0; nDone < n; nDone += LINE) {
      int m = Math.min(n - nDone, LINE);
      for (int i = 0; i < m; i++) {
        seed = (seed * IA + IC) % IM;
        double r = (double) seed / IM;
        int j = 0;
        while (j < nSym - 1 && r >= aCum[j]) j++;
        out.append(cSym.charAt(j));
      }
      out.append('\n');
    }
    seedBox[0] = seed;
    return out.toString();
  }

  // The three sections of fasta for n: the ALU sequence over 2n characters, 3n IUB codes, 5n Homo sapiens bases.
  static String fasta(int n) {
    long[] seed = {42};
    return ">ONE Homo sapiens alu\n" + repeatSeq(2 * n) + ">TWO IUB ambiguity codes\n"
        + randomSeq(seed, "acgtBDHKMNRSVWY",
            new double[] {0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02},
            3 * n)
        + ">THREE Homo sapiens frequency\n"
        + randomSeq(seed, "acgt", new double[] {0.3029549426680, 0.1979883004921, 0.1975473066391,
            0.3015094502008}, 5 * n);
  }

  public static void main(String[] args) {
    String in = fasta(2500000);
    long t = System.nanoTime();
    char[] comp = new char[256];
    String cFrom = "ACBDGHKMNSRUTWVYacbdghkmnsrutwvy", cTo = "TGVHCDMKNSYAABWRTGVHCDMKNSYAABWR";
    for (int i = 0; i < 256; i++) comp[i] = (char) i;
    for (int i = 0; i < cFrom.length(); i++) comp[cFrom.charAt(i)] = cTo.charAt(i);
    StringBuilder out = new StringBuilder(), seq = new StringBuilder();
    int i = 0;
    while (i < in.length()) {
      char c = in.charAt(i);
      if (c == '>') {
        if (seq.length() > 0) {
          out.append(revComp(seq, comp));
          seq.setLength(0);
        }
        while ((c = in.charAt(i++)) != '\n') out.append(c);
        out.append(c);
      } else {
        if (c != '\n') seq.append(c);
        i++;
      }
    }
    out.append(revComp(seq, comp));
    String s = out.toString();
    int nTwo = s.indexOf(">TWO"), nThree = s.indexOf(">THREE");
    boolean ok = s.length() == 25416745
        && s.startsWith("GTAGCTGGGATTACAGGCGCGCGCCACCACGCCCGGCTAATTTTTGTATTTTTAGTAGAG", 22)
        && s.startsWith("TTTATTYSTTGTATCCCTMWCGAACTAVGYHTATANTAWCARAWTTTVCTSTKDNTATHA", nTwo + 25)
        && s.startsWith("GTAATTTCTCTCGGCTATTAGATTTGAACATATACACAGGCCAGATGATCTGGAAAAATC", nThree + 30)
        && s.endsWith("CTATAACTAATATATTCGTC\n");
    Bench.report("reverse_complement", t, ok);
  }
}
