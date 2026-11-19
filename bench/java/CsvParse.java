// bench/csv_parse.prg in Java: CSV parsed character by character: a 40,000-line text (about 2 MB) of 5 fields, some
// quoted with commas, doubled quotes and line breaks inside, read by a state machine into a list of records (fields
// built in a StringBuilder); numbers read with Long.parseLong; 6 rounds.
import java.util.ArrayList;

public class CsvParse {
  static long seed = 1999;

  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  static ArrayList<ArrayList<String>> parseCsv(String c) {
    ArrayList<ArrayList<String>> recs = new ArrayList<>();
    ArrayList<String> rec = new ArrayList<>();
    StringBuilder field = new StringBuilder();
    boolean quoted = false;
    int len = c.length();
    for (int i = 0; i < len; i++) {
      char x = c.charAt(i);
      if (quoted) {
        if (x == '"') {
          if (i + 1 < len && c.charAt(i + 1) == '"') {
            field.append(x);
            i++;
          } else {
            quoted = false;
          }
        } else {
          field.append(x);
        }
      } else if (x == '"') {
        quoted = true;
      } else if (x == ',') {
        rec.add(field.toString());
        field.setLength(0);
      } else if (x == '\n') {
        rec.add(field.toString());
        recs.add(rec);
        rec = new ArrayList<>();
        field.setLength(0);
      } else {
        field.append(x);
      }
    }
    return recs;
  }

  // A field: plain letters, or quoted with a comma, a doubled quote or a line break inside.
  static String makeField() {
    long k = rand(6);
    String w = String.valueOf((char) (97 + rand(26))).repeat((int) (3 + rand(12)));
    if (k == 0) return "\"" + w + ", " + w + "\"";
    if (k == 1) return "\"say \"\"" + w + "\"\"\"";
    if (k == 2) return "\"" + w + "\n" + w + "\"";
    return w;
  }

  public static void main(String[] args) {
    StringBuilder sb = new StringBuilder();
    for (long i = 1; i <= 40000; i++) {
      String a = makeField(), b = Long.toString(rand(100000)), d = makeField();
      sb.append(i).append(',').append(a).append(',').append(b).append(',').append(d).append(',')
          .append("w".repeat((int) rand(10))).append('\n');
    }
    String c = sb.toString();
    long t = System.nanoTime(), n = 0, quotedCount = 0;
    ArrayList<ArrayList<String>> recs = null;
    for (int r = 1; r <= 6; r++) {
      recs = parseCsv(c);
      for (ArrayList<String> a : recs) {
        n += Long.parseLong(a.get(0)) + Long.parseLong(a.get(2)) + a.get(1).length() + a.get(3).length() + a.size();
        if (a.get(3).indexOf('\n') >= 0 || a.get(1).indexOf('"') >= 0) quotedCount++;
      }
    }
    Bench.report("csv_parse", t, recs.size() == 40000 && n == 16750579326L && quotedCount == 73038);
  }
}
