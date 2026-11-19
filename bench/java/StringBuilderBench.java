// bench/string_builder.prg in Java: 3 rounds of 400,000 table rows of about 37 bytes appended to one text
// (a StringBuilder, about 15 MB each), its length and every 1,000th byte checked.
public class StringBuilderBench {
  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    String[] aNames = {"ant", "bee", "cat", "dog", "eel", "fox", "gnu", "hen", "ibis", "jay"};
    for (int r = 1; r <= 3; r++) {
      StringBuilder c = new StringBuilder();
      for (int i = 1; i <= 400000; i++)
        c.append("<tr><td>").append(aNames[i % 10]).append("</td><td>").append(i + r).append("</td></tr>").append('\n');
      n += c.length();
      for (int i = 0; i < c.length(); i += 1000) n += c.charAt(i);
    }
    Bench.report("string_builder", t, n == 47588053);
  }
}
