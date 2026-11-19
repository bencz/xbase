// bench/date_time_ops.prg in Java (java.time): 200,000 dates between 1900 and 2099 and times of day from a fixed
// seed. For each: year, month, day against the yyyymmdd text; the day of the week and its English name (Sundays,
// Fridays the 13th); dd/mm/yyyy text and back; 13 months added (the day kept within the month, through yyyymmdd
// text and date arithmetic); the days to the year's end; a time of day as hh:mm:ss text and back.
import java.time.DayOfWeek;
import java.time.LocalDate;
import java.time.LocalTime;
import java.time.format.DateTimeFormatter;
import java.time.format.TextStyle;
import java.util.Locale;

public class DateTimeOps {
  static final DateTimeFormatter YMD = DateTimeFormatter.BASIC_ISO_DATE;
  static final DateTimeFormatter BRITISH = DateTimeFormatter.ofPattern("dd/MM/uuuu");
  static final DateTimeFormatter TIME = DateTimeFormatter.ofPattern("HH:mm:ss");
  static long seed = 1999;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  // The date n months after y-m-d, its day made the month's last when the month is shorter.
  static LocalDate addMonths(int y, int m, int nDay, int n) {
    int nM = m + n;
    int nY = y + (nM - 1) / 12;
    nM = (nM - 1) % 12 + 1;
    LocalDate dFirst = LocalDate.parse(String.format(Locale.ROOT, "%04d%02d01", nY, nM), YMD);
    LocalDate dNext = dFirst.plusDays(31);
    return dFirst.plusDays(Math.min(nDay, dNext.minusDays(dNext.getDayOfMonth()).getDayOfMonth()) - 1);
  }

  public static void main(String[] args) {
    LocalDate dBase = LocalDate.parse("19000101", YMD);
    long nYmd = 0, nSun = 0, nFri13 = 0, nAdd = 0, nToEnd = 0, nSec = 0, nBad = 0;
    long t = System.nanoTime();
    for (int i = 1; i <= 200000; i++) {
      LocalDate d = dBase.plusDays(rand(73000));
      int y = d.getYear(), m = d.getMonthValue(), nDay = d.getDayOfMonth();
      long n = y * 10000L + m * 100L + nDay;
      nYmd += n;
      if (Long.parseLong(d.format(YMD)) != n || !LocalDate.parse(d.format(BRITISH), BRITISH).equals(d)) nBad++;
      if (d.getDayOfWeek() == DayOfWeek.SUNDAY) nSun++;
      if (nDay == 13 && d.getDayOfWeek().getDisplayName(TextStyle.FULL, Locale.ENGLISH).equals("Friday")) nFri13++;
      nAdd += Long.parseLong(addMonths(y, m, nDay, 13).format(YMD));
      nToEnd += LocalDate.parse(y + "1231", YMD).toEpochDay() - d.toEpochDay();
      long s = rand(86400);
      if (LocalTime.parse(LocalTime.ofSecondOfDay(s).format(TIME), TIME).toSecondOfDay() != s) nBad++;
      nSec += s;
    }
    boolean ok = nBad == 0 && nYmd == 3998853334023L && nSun == 28567 && nFri13 == 951 && nAdd == 4001021793716L
        && nToEnd == 36531947 && nSec == 8632784536L;
    Bench.report("date_time_ops", t, ok);
  }
}
