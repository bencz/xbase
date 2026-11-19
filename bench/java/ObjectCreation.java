// bench/object_creation.prg in Java: creating objects: 600,000 objects of a class with five fields (initial values:
// numbers, a string and a list, which each object gets its own of) made by a constructor that sets two of them, half
// of them of a subclass with two more fields; one in 100 kept in a list, the rest left to the collector.
import java.time.LocalDate;
import java.util.ArrayList;

public class ObjectCreation {
  static class TOrder {
    long nId = 0;
    String cCustomer = "";
    long nTotal = 0;
    String cStatus = "new";
    ArrayList<Long> aLines = new ArrayList<>();

    TOrder(long nId, String cCustomer) {
      this.nId = nId;
      this.cCustomer = cCustomer;
    }
  }

  static class TBigOrder extends TOrder {
    long nDiscount = 0;
    LocalDate dDue = LocalDate.of(2026, 12, 31);

    TBigOrder(long nId, String cCustomer) { super(nId, cCustomer); }
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    ArrayList<TOrder> keep = new ArrayList<>();
    for (long i = 1; i <= 600000; i++) {
      TOrder o;
      if (i % 2 == 0) {
        o = new TOrder(i, "c" + String.format("%03d", i % 1000));
      } else {
        TBigOrder b = new TBigOrder(i, "c000");
        b.nDiscount = 5;
        o = b;
      }
      o.aLines.add(i % 7);
      if (i % 100 == 0) keep.add(o);
    }
    for (TOrder o : keep) n += o.nId + o.nTotal + o.aLines.size() + o.aLines.get(0) + o.cCustomer.length();
    // the kept ones: ids 100, 200, ..., 600,000 (all even: TOrder), nTotal 0, one line of i % 7, a 4-character name;
    // 17999 is the sum of i % 7 over those ids
    Bench.report("object_creation", t, keep.size() == 6000 && n == 1800300000L + 6000 * 5 + 17999);
  }
}
