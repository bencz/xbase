// bench/log_processing.prg in Java: log processing: 300,000 lines of an access log (time, level, [module], user,
// duration, status) made from a fixed seed as one text, then timed: split into lines with split, each line parsed
// (substring at fixed columns, indexOf for the module and the key=value fields, the leading number of a substring as
// Val() reads it), lines counted per level in a HashMap, durations summed per module, the slowest request found,
// server errors counted per hour, distinct users counted in a HashMap.
import java.util.HashMap;

public class LogProcessing {
  static long seed = 9001;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static String zero(long n, int width) {
    return String.format("%0" + width + "d", n);
  }

  // Val(): the number at the start of s (leading blanks skipped), 0 when there is none.
  static long val(String s) {
    int i = 0, n = s.length();
    long v = 0;
    while (i < n && s.charAt(i) == ' ') i++;
    for (; i < n && s.charAt(i) >= '0' && s.charAt(i) <= '9'; i++) v = v * 10 + (s.charAt(i) - '0');
    return v;
  }

  public static void main(String[] args) {
    String[] lv = {"INFO ", "WARN ", "ERROR", "DEBUG"}, mod = {"auth", "db", "http", "cache", "queue", "mail"};
    long[] st = {200, 200, 200, 201, 204, 301, 404, 500, 503, 200}, hourErr = new long[24];
    int n = 300000;
    String[] lines = new String[n];
    for (int i = 1; i <= n; i++) {
      long sec = (long) i * 86400 / n, x = rand(1000), l = rand(10);
      l = l < 6 ? 1 : l < 8 ? 4 : l < 9 ? 2 : 3;
      String m = mod[(int) rand(6)], user = Long.toString(rand(5000)), ms = Long.toString(rand(2000));
      lines[i - 1] = "2026-11-19 " + zero(sec / 3600, 2) + ":" + zero(sec / 60 % 60, 2) + ":" + zero(sec % 60, 2) + "."
          + zero(x, 3) + " " + lv[(int) l - 1] + " [" + m + "] request done user=u" + user + " ms=" + ms + " status="
          + String.format("%3d", st[(int) rand(10)]);
    }
    StringBuilder sb = new StringBuilder();
    for (String x : lines) sb.append(x).append('\n');
    String c = sb.toString();
    lines = null;
    long t = System.nanoTime(), max = -1, maxLine = 0, err5 = 0, i = 0;
    HashMap<String, Long> hLv = new HashMap<>(), hMod = new HashMap<>();
    HashMap<String, Boolean> hUsers = new HashMap<>();
    for (String x : c.split("\n")) {
      if (x.isEmpty()) continue;
      i++;
      String l = x.substring(24, 29).stripTrailing();
      hLv.put(l, hLv.getOrDefault(l, 0L) + 1);
      int b = x.indexOf('[');
      String cMod = x.substring(b + 1, x.indexOf(']'));
      long ms = val(x.substring(x.indexOf(" ms=") + 4));
      hMod.put(cMod, hMod.getOrDefault(cMod, 0L) + ms);
      if (ms > max) {
        max = ms;
        maxLine = i;
      }
      long status = val(x.substring(x.indexOf(" status=") + 8));
      if (status >= 500) {
        err5++;
        hourErr[(int) val(x.substring(11, 13))]++;
      }
      hUsers.put(x.substring(x.indexOf(" user=") + 6, x.indexOf(" ms=")), true);
    }
    boolean ok = c.length() == 23765927 && i == n && hLv.get("INFO") == 179990 && hLv.get("WARN") == 29916
        && hLv.get("ERROR") == 30173 && hLv.get("DEBUG") == 59921 && hMod.get("auth") == 50005329
        && hMod.get("db") == 50599071 && hMod.get("http") == 50094170 && hMod.get("cache") == 49851623
        && hMod.get("queue") == 49898190 && hMod.get("mail") == 49855051 && max == 1999 && maxLine == 1062
        && err5 == 59808 && hourErr[13] == 2503 && hUsers.size() == 5000;
    Bench.report("log_processing", t, ok);
  }
}
