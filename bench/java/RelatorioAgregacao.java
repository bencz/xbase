// bench/relatorio_agregacao.prg in Java: 1,000,000 sales records (region, product, quantity, unit price in cents)
// from a fixed seed, aggregated by region and by product in hash maps keyed by code, the 500 products ordered by
// amount with the list's sort and a comparator (ties by code), and a report built with String.format in the German
// locale (the .prg's Transform with @E pictures): a line per region (sales, quantity, amount) and the top 20 products.
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.TreeMap;

public class RelatorioAgregacao {
  static long seed = 12345;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static final class Sale {
    final String region, product;
    final long qty, price;
    Sale(String region, String product, long qty, long price) {
      this.region = region;
      this.product = product;
      this.qty = qty;
      this.price = price;
    }
  }

  public static void main(String[] args) {
    Sale[] sales = new Sale[1000000];
    for (int i = 0; i < sales.length; i++) {
      String region = String.format(Locale.ROOT, "R%02d", rand(8) + 1);
      String product = String.format(Locale.ROOT, "P%04d", rand(500) + 1);
      long qty = rand(20) + 1;
      sales[i] = new Sale(region, product, qty, rand(100000) + 100);
    }
    long t = System.nanoTime(), total = 0;
    Map<String, long[]> reg = new HashMap<>();
    Map<String, Long> prod = new HashMap<>();
    for (Sale r : sales) {
      long x = r.qty * r.price;
      long[] a = reg.get(r.region);
      if (a != null) {
        a[0]++;
        a[1] += r.qty;
        a[2] += x;
      } else {
        reg.put(r.region, new long[] {1, r.qty, x});
      }
      prod.put(r.product, prod.getOrDefault(r.product, 0L) + x);
      total += x;
    }
    List<Map.Entry<String, Long>> aProd = new ArrayList<>(prod.entrySet());
    aProd.sort((p, q) -> p.getValue() > q.getValue() || (p.getValue().equals(q.getValue())
        && p.getKey().compareTo(q.getKey()) < 0) ? -1 : 1);
    StringBuilder rep = new StringBuilder();
    for (Map.Entry<String, long[]> e : new TreeMap<>(reg).entrySet()) {
      long[] a = e.getValue();
      rep.append(e.getKey()).append(String.format(Locale.GERMANY, "%,9d%,10d%,18.2f", a[0], a[1], a[2] / 100.0))
          .append('\n');
    }
    for (int i = 0; i < 20; i++) {
      Map.Entry<String, Long> p = aProd.get(i);
      rep.append(p.getKey()).append(String.format(Locale.GERMANY, "%,18.2f", p.getValue() / 100.0)).append('\n');
    }
    String[] lines = rep.toString().split("\n", -1);
    boolean ok = total == 525748851783L && rep.length() == 8 * 41 + 20 * 24 && prod.size() == 500
        && lines[0].equals("R01  125.691 1.320.222    662.846.057,84")
        && lines[7].equals("R08  124.683 1.310.936    656.013.155,38")
        && lines[8].equals("P0303     11.438.921,18") && lines[27].equals("P0360     11.021.303,35");
    Bench.report("relatorio_agregacao", t, ok);
  }
}
