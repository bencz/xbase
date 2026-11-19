// bench/pivot_crosstab.prg in Java: 2,000,000 sales records { seller, date, amount } from a fixed seed (60 sellers,
// the days of 2025), then the amounts summed per seller and month in a HashMap of rows and per weekday and month;
// row and column totals, the sellers sorted, and the table rendered as text (padded names, numbers right-aligned)
// with a totals line.
import java.time.LocalDate;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Locale;

public class PivotCrosstab {
  static long seed = 16180;

  static int rand(int n) {
    seed = seed * 16807 % 2147483647;
    return (int) (seed % n);
  }

  record Sale(String seller, LocalDate date, long amount) {}

  public static void main(String[] args) {
    LocalDate dBase = LocalDate.of(2025, 1, 1);
    Sale[] aSales = new Sale[2000000];
    for (int i = 0; i < aSales.length; i++) {
      String seller = String.format(Locale.ROOT, "SELLER%03d", rand(60) + 1);
      LocalDate date = dBase.plusDays(rand(365));
      aSales[i] = new Sale(seller, date, rand(50000) + 1);
    }
    long t = System.nanoTime(), nSat = 0, nGrand = 0, nBest = 0;
    HashMap<String, long[]> hRows = new HashMap<>();
    long[] aCol = new long[12];
    long[][] aDow = new long[7][12];
    for (Sale r : aSales) {
      long[] a = hRows.get(r.seller());
      if (a == null) {
        a = new long[12];
        hRows.put(r.seller(), a);
      }
      int nMonth = r.date().getMonthValue() - 1;
      a[nMonth] += r.amount();
      aDow[r.date().getDayOfWeek().getValue() % 7][nMonth] += r.amount(); // Sunday first, as DoW()
    }
    ArrayList<String> keys = new ArrayList<>(hRows.keySet());
    Collections.sort(keys);
    StringBuilder cTab = new StringBuilder();
    String cBest = null, cS07 = null;
    for (String k : keys) {
      long[] a = hRows.get(k);
      StringBuilder cLine = new StringBuilder(String.format(Locale.ROOT, "%-10s", k));
      long nRow = 0;
      for (int i = 0; i < 12; i++) {
        cLine.append(String.format(Locale.ROOT, "%11d", a[i]));
        nRow += a[i];
        aCol[i] += a[i];
      }
      cLine.append(String.format(Locale.ROOT, "%13d", nRow));
      if (nRow > nBest) {
        cBest = k;
        nBest = nRow;
      }
      if (k.equals("SELLER007")) cS07 = cLine.toString();
      cTab.append(cLine).append('\n');
    }
    StringBuilder cLine = new StringBuilder(String.format(Locale.ROOT, "%-10s", "TOTAL"));
    for (int i = 0; i < 12; i++) {
      cLine.append(String.format(Locale.ROOT, "%11d", aCol[i]));
      nGrand += aCol[i];
    }
    cTab.append(cLine).append(String.format(Locale.ROOT, "%13d", nGrand)).append('\n');
    for (long x : aDow[6]) nSat += x;
    boolean ok = nGrand == 50028051852L && "SELLER015".equals(cBest) && nBest == 844241139
        && aCol[0] == 4275964399L && aCol[1] == 3837132322L && aCol[11] == 4246237533L
        && aDow[0][0] == 547728957 && nSat == 7111132032L
        && cTab.length() == 61 * (10 + 12 * 11 + 13 + 1) && ("SELLER007 "
        + "   71382096   62515370   68284183   68152167   70863968   69201960"
        + "   71447433   71672784   67917922   68878748   65718083   68477906    824512620").equals(cS07);
    Bench.report("pivot_crosstab", t, ok);
  }
}
