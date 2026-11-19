// bench/string_concatenation.prg in Java: 3,000,000 strings of seven parts joined with +, picked by index from small
// tables, each one's length and last character read.
public class StringConcatenation {
  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    String[] aA = {"alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta"};
    String[] aB = {"red", "green", "blue", "cyan", "magenta"};
    for (int i = 1; i <= 3000000; i++) {
      String s = aA[i % 7] + "-" + aB[i % 5] + "/" + aA[i % 3] + ":" + aB[i % 4];
      n += s.length() + s.charAt(s.length() - 1);
    }
    Bench.report("string_concatenation", t, n == 378692857);
  }
}
