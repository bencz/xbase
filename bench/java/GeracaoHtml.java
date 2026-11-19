// bench/geracao_html.prg in Java: a table of 100,000 items { name of two words, quantity, price in cents } from a
// fixed seed, then one HTML page built in a StringBuilder, each row with its id, a zebra class, the name escaped
// (replace of & < > ") and the price and total formatted with 2 decimals; its length, a row and its end checked.
import java.util.Locale;

public class GeracaoHtml {
  static long seed = 4711;

  static int rand(int n) {
    seed = seed * 16807 % 2147483647;
    return (int) (seed % n);
  }

  static String escape(String c) {
    return c.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;");
  }

  record Item(String name, long qty, long cents) {}

  public static void main(String[] args) {
    String[] aWords = {"Alpha", "Beta & Sons", "<Gamma>", "Delta", "Echo \"Q\"", "Foxtrot", "Golf", "H&M"};
    Item[] aItems = new Item[100000];
    for (int i = 0; i < aItems.length; i++) {
      String name = aWords[rand(8)] + " " + aWords[rand(8)];
      long qty = rand(100) + 1;
      aItems[i] = new Item(name, qty, rand(1000000) + 1);
    }
    long t = System.nanoTime();
    int nPos77 = 0;
    StringBuilder c = new StringBuilder("<html><head><title>Items</title></head><body>\n<table>\n"
        + "<tr><th>Id</th><th>Name</th><th>Qty</th><th>Price</th><th>Total</th></tr>\n");
    for (int i = 1; i <= aItems.length; i++) {
      Item r = aItems[i - 1];
      String cId = Integer.toString(i);
      if (i == 77777) nPos77 = c.length();
      String cRow = "<tr id=\"r" + cId + "\" class=\"" + (i % 2 == 1 ? "odd" : "even") + "\"><td>" + cId + "</td><td>"
          + escape(r.name()) + "</td><td class=\"n\">" + r.qty() + "</td><td class=\"n\">"
          + String.format(Locale.ROOT, "%.2f", r.cents() / 100.0) + "</td><td class=\"n\">"
          + String.format(Locale.ROOT, "%.2f", r.qty() * r.cents() / 100.0) + "</td></tr>\n";
      c.append(cRow);
    }
    c.append("</table>\n</body></html>\n");
    String cRow77 = "<tr id=\"r77777\" class=\"odd\"><td>77777</td><td>&lt;Gamma&gt; Delta</td><td class=\"n\">56</td>"
        + "<td class=\"n\">3992.87</td><td class=\"n\">223600.72</td></tr>\n";
    String cTail = "<tr id=\"r100000\" class=\"even\"><td>100000</td><td>Foxtrot Alpha</td><td class=\"n\">57</td>"
        + "<td class=\"n\">4506.67</td><td class=\"n\">256880.19</td></tr>\n</table>\n</body></html>\n";
    String s = c.toString();
    boolean ok = s.length() == 15121698 && nPos77 + 1 == 11756338 && s.startsWith(cRow77, nPos77)
        && s.endsWith(cTail);
    Bench.report("geracao_html", t, ok);
  }
}
