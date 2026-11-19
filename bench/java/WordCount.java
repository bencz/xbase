// bench/word_count.prg in Java: word frequencies: 200,000 lines of 1 to 30 words (about 1,300,000 words, of a
// 5,000-word vocabulary in mixed case and a skewed distribution) cut at blanks (indexOf and substring, the rest of the
// line taken each time), lower-cased and counted in a HashMap; the most frequent word found. The word and line
// lengths are drawn again at every test of their loops, as the .prg's FOR evaluates its limit.
import java.util.HashMap;
import java.util.Locale;

public class WordCount {
  static long seed = 4242;

  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  public static void main(String[] args) {
    String letters = "eEtaAoinNsrhldcumfpgwybvkxjqz";
    String[] vocab = new String[5000], lines = new String[200000];
    for (int i = 0; i < vocab.length; i++) {
      StringBuilder w = new StringBuilder();
      for (long j = 1; j <= 2 + rand(9); j++) w.append(letters.charAt((int) rand(rand(29) + 1)));
      vocab[i] = w.toString();
    }
    for (int i = 0; i < lines.length; i++) {
      StringBuilder c = new StringBuilder();
      for (long j = 1; j <= 1 + rand(30); j++) c.append(vocab[(int) rand(rand(5000) + 1)]).append(' ');
      lines[i] = c.toString();
    }
    long t = System.nanoTime(), words = 0, top = 0, check = 0;
    HashMap<String, Long> h = new HashMap<>();
    for (String c : lines) {
      int n;
      while ((n = c.indexOf(' ')) >= 0) {
        String w = c.substring(0, n).toLowerCase(Locale.ROOT);
        c = c.substring(n + 1);
        words++;
        Long k = h.get(w);
        h.put(w, k == null ? 1 : k + 1);
      }
    }
    for (long n : h.values()) {
      check += n * n;
      if (n > top) top = n;
    }
    String cTop = "";
    for (String v : vocab) {
      Long k = h.get(v.toLowerCase(Locale.ROOT));
      if (k != null && k == top) {
        cTop = v.toLowerCase(Locale.ROOT);
        break;
      }
    }
    Bench.report("word_count", t, words == 1312183 && h.size() == 4328 && top == 7336 && cTop.equals("ee")
        && check == 971695061);
  }
}
