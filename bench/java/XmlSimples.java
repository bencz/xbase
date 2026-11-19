// bench/xml_simples.prg in Java: a catalog of 50,000 books (titles and authors with & < > and ' in them) from a fixed
// seed, written as one XML document (the text escaped), split into elements and read back (each child's text found
// with indexOf between its tags, the id attribute, the entities decoded with replace, the price parsed).
import java.util.Locale;
import java.util.regex.Pattern;

public class XmlSimples {
  static long seed = 5150;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  // The text between <tag> and </tag> in c.
  static String inner(String c, String tag) {
    int start = c.indexOf("<" + tag + ">") + tag.length() + 2;
    return c.substring(start, c.indexOf("</" + tag + ">"));
  }

  static String escape(String c) {
    return c.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("'", "&apos;");
  }

  static String unescape(String c) {
    return c.replace("&apos;", "'").replace("&gt;", ">").replace("&lt;", "<").replace("&amp;", "&");
  }

  // The number at the start of c (as Val() reads it).
  static long leadingNumber(String c) {
    int n = 0;
    while (n < c.length() && Character.isDigit(c.charAt(n))) n++;
    return Long.parseLong(c.substring(0, n));
  }

  public static void main(String[] args) {
    String[] words = {"Rivers", "Salt & Pepper", "<Night>", "Stone", "Tales", "Tom & Jerry", "Glass", "Winter", "Red",
      "Old"};
    String[] authors = {"Ana Lima", "Bruno Costa", "Carla Dias", "Diego Reis", "Eva O'Neil"};
    String[] titles = new String[50000], names = new String[50000];
    long[] prices = new long[50000], years = new long[50000];
    for (int i = 0; i < titles.length; i++) {
      long n = rand(2) + 2;
      StringBuilder title = new StringBuilder();
      for (long k = 1; k <= n; k++) title.append(k > 1 ? " " : "").append(words[(int) rand(10)]);
      titles[i] = title.toString();
      names[i] = authors[(int) rand(5)];
      prices[i] = rand(10000) + 100;
      years[i] = 1950 + rand(75);
    }
    long t = System.nanoTime(), cents = 0, amp = 0, yearSum = 0, eva = 0, ids = 0;
    StringBuilder sb = new StringBuilder("<?xml version=\"1.0\"?>\n<catalog>\n");
    for (int i = 0; i < titles.length; i++)
      sb.append("<book id=\"").append(i + 1).append("\"><title>").append(escape(titles[i])).append("</title><author>")
          .append(escape(names[i])).append("</author><price>")
          .append(String.format(Locale.ROOT, "%.2f", prices[i] / 100.0)).append("</price><year>").append(years[i])
          .append("</year></book>\n");
    sb.append("</catalog>\n");
    String c = sb.toString(), c12345 = null;
    boolean ok = c.length() == 6276273;
    String[] parts = c.split(Pattern.quote("<book "), -1);
    for (int i = 1; i < parts.length; i++) {
      String book = parts[i];
      ids += leadingNumber(book.substring(4));
      String title = unescape(inner(book, "title"));
      if (title.contains("&")) amp++;
      if (i == 12345) c12345 = title;
      if (unescape(inner(book, "author")).equals("Eva O'Neil")) eva++;
      cents += Math.round(Double.parseDouble(inner(book, "price")) * 100);
      yearSum += Long.parseLong(inner(book, "year"));
    }
    ok = ok && parts.length == 50001 && ids == 50000L * 50001 / 2 && cents == 255248286 && amp == 21225
        && yearSum == 99351507 && eva == 10152 && "Winter Glass Tom & Jerry".equals(c12345);
    Bench.report("xml_simples", t, ok);
  }
}
