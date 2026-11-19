// bench/tokenize_split.prg in Java: 200,000 comma-separated lines of 6 fields cut with indexOf() and substring() into
// lists (written out, the rest of the line cut again after each field), the numeric fields parsed; 2 rounds. The
// numbers have 2 decimals, as the .prg's Str() shows its random numbers (remainders, which have SET DECIMALS ones).
import java.util.ArrayList;
import java.util.List;

public class TokenizeSplit {
  static long seed = 64;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static List<String> split(String c, String sep) {
    List<String> a = new ArrayList<>();
    int n;
    while ((n = c.indexOf(sep)) >= 0) {
      a.add(c.substring(0, n));
      c = c.substring(n + 1);
    }
    a.add(c);
    return a;
  }

  public static void main(String[] args) {
    String[] lines = new String[200000];
    for (int i = 0; i < lines.length; i++)
      lines[i] = rand(100000) + ".00,name" + (i + 1) + "," + rand(1000) + ".00,," + "z".repeat((int) rand(12)) + ",end";
    long t = System.nanoTime();
    double n = 0;
    for (int r = 1; r <= 2; r++)
      for (String c : lines) {
        List<String> a = split(c, ",");
        n += Double.parseDouble(a.get(0)) + Double.parseDouble(a.get(2)) + a.size() + a.get(4).length();
      }
    Bench.report("tokenize_split", t, n == 20190509958.0);
  }
}
