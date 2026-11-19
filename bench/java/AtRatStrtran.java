// bench/at_rat_strtran.prg in Java: indexOf() and lastIndexOf() (At() and RAt(), 1-based) of 200 words in a
// 20,000-byte text of those words, and replace() of each word by a longer one; 80 rounds.
public class AtRatStrtran {
  static long seed = 33;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  public static void main(String[] args) {
    String[] words = new String[200];
    for (int i = 0; i < words.length; i++) {
      StringBuilder w = new StringBuilder();
      for (long j = 1; j <= 3 + rand(6); j++) w.append((char) (97 + rand(26)));
      words[i] = w.toString();
    }
    StringBuilder text = new StringBuilder();
    while (text.length() < 20000) text.append(words[(int) rand(rand(200) + 1)]).append(' ');
    String c = text.toString();
    long t = System.nanoTime(), n = 0;
    for (int r = 1; r <= 80; r++)
      for (String w : words) n += c.indexOf(w) + 1 + c.lastIndexOf(w) + 1 + c.replace(w, "<" + w + ">").length();
    Bench.report("at_rat_strtran", t, n == 620527280);
  }
}
