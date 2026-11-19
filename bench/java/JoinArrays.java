// bench/join_arrays.prg in Java: joins of two arrays of records, 20,000 customers { id, name, segment, city } and
// 500,000 orders { id, customer, amount } made from a fixed seed (a third of the orders name a customer that exists).
// Timed: a hash join (the customers in a HashMap by id, each order looked up; amounts summed per segment, orders
// counted per city in a HashMap), then a sort-merge join (the orders' customer and position packed in one long key,
// sorted with Arrays.sort, merged with the customers in id order; the amounts weighted by segment).
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

public class JoinArrays {
  record Customer(long id, String name, int segment, String city) {}

  record Order(long id, long customer, long amount) {}

  static long seed = 2718;
  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  public static void main(String[] args) {
    Customer[] cust = new Customer[20000];
    Order[] ord = new Order[500000];
    for (int i = 1; i <= cust.length; i++) {
      int segment = (int) rand(5) + 1;
      cust[i - 1] = new Customer(i * 3L, "Customer " + i, segment, "CITY" + String.format("%03d", rand(100) + 1));
    }
    for (int i = 1; i <= ord.length; i++) {
      long c = rand(60000) + 1;
      ord[i - 1] = new Order(i, c, rand(100000) + 1);
    }
    long t = System.nanoTime(), matched = 0, w = 0, merged = 0, best = 0;
    long[] seg = new long[5];
    HashMap<Long, Customer> hCust = new HashMap<>();
    HashMap<String, Long> hCity = new HashMap<>();
    String cBest = null;
    for (Customer c : cust) hCust.put(c.id(), c);
    for (Order o : ord) {
      Customer c = hCust.get(o.customer());
      if (c != null) {
        seg[c.segment() - 1] += o.amount();
        hCity.merge(c.city(), 1L, Long::sum);
        matched++;
      }
    }
    for (Map.Entry<String, Long> e : hCity.entrySet()) {
      long n = e.getValue();
      if (n > best || (n == best && e.getKey().compareTo(cBest) < 0)) {
        cBest = e.getKey();
        best = n;
      }
    }
    long[] keys = new long[ord.length];
    for (int i = 0; i < ord.length; i++) keys[i] = ord[i].customer() * 1000000 + i + 1;
    Arrays.sort(keys);
    int pos = 0;
    for (long k : keys) {
      Order o = ord[(int) (k % 1000000) - 1];
      while (pos < cust.length && cust[pos].id() < o.customer()) pos++;
      if (pos < cust.length && cust[pos].id() == o.customer()) {
        w += o.amount() * cust[pos].segment();
        merged++;
      }
    }
    boolean ok = matched == 166641 && merged == 166641 && w == 25122891088L && seg[0] == 1626943956
        && seg[1] == 1648544230 && seg[2] == 1673211091 && seg[3] == 1694374411 && seg[4] == 1680345551
        && "CITY080".equals(cBest) && best == 1980 && hCity.size() == 100;
    Bench.report("join_arrays", t, ok);
  }
}
