// bench/validacao_dados.prg in Java: data validation, 250,000 records { CPF, e-mail, date text, amount text } made
// from a fixed seed, with errors mixed in (a wrong check digit, an e-mail with no "@", no dot or a bad character,
// impossible dates, a letter among the digits). Timed: each field validated by its rule (the CPF's two mod-11 check
// digits, the e-mail's parts and characters, the date dd/mm/yyyy through a strict DateTimeFormatter, the amount's
// digits and range) and the valid ones counted.
import java.time.LocalDate;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeParseException;
import java.time.format.ResolverStyle;

public class ValidacaoDados {
  static final DateTimeFormatter BRITISH = DateTimeFormatter.ofPattern("dd/MM/uuuu").withResolverStyle(
      ResolverStyle.STRICT);
  static long seed = 8086;
  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static boolean cpfOk(String c) {
    String d = c.replace(".", "").replace("-", "");
    if (d.length() != 11) return false;
    for (int i = 0; i < 11; i++)
      if (!isDigit(d.charAt(i))) return false;
    long s = 0;
    for (int i = 1; i <= 9; i++) s += (d.charAt(i - 1) - 48) * (11 - i);
    if (s * 10 % 11 % 10 != d.charAt(9) - 48) return false;
    s = 0;
    for (int i = 1; i <= 10; i++) s += (d.charAt(i - 1) - 48) * (12 - i);
    return s * 10 % 11 % 10 == d.charAt(10) - 48;
  }

  static boolean isDigit(char c) { return c >= '0' && c <= '9'; }

  static boolean emailOk(String e) {
    int at = e.indexOf('@') + 1;
    if (at < 2 || e.indexOf('@', at) >= 0) return false;
    for (int i = 0; i < at - 1; i++)
      if ("abcdefghijklmnopqrstuvwxyz0123456789._".indexOf(e.charAt(i)) < 0) return false;
    String dom = e.substring(at);
    if (dom.length() < 3 || dom.indexOf('.') < 0 || dom.startsWith(".") || dom.endsWith(".")) return false;
    for (int i = 0; i < dom.length(); i++)
      if ("abcdefghijklmnopqrstuvwxyz0123456789.-".indexOf(dom.charAt(i)) < 0) return false;
    return true;
  }

  static boolean dateOk(String s) {
    try {
      LocalDate.parse(s, BRITISH);
      return true;
    } catch (DateTimeParseException e) {
      return false;
    }
  }

  static boolean amountOk(String c) {
    for (int i = 0; i < c.length(); i++)
      if (!isDigit(c.charAt(i))) return false;
    long n = Long.parseLong(c);
    return n >= 1 && n <= 999999;
  }

  // A record; its errors come from the seed as the checks' constants were computed.
  static String[] makeRecord() {
    long[] d = new long[11];
    long s = 0;
    for (int k = 0; k < 9; k++) d[k] = rand(10);
    for (int k = 1; k <= 9; k++) s += d[k - 1] * (11 - k);
    d[9] = s * 10 % 11 % 10;
    s = 0;
    for (int k = 1; k <= 10; k++) s += d[k - 1] * (12 - k);
    d[10] = s * 10 % 11 % 10;
    if (rand(4) == 0) d[10] = (d[10] + 1) % 10;
    StringBuilder cpf = new StringBuilder();
    for (int k = 1; k <= 11; k++) cpf.append(d[k - 1]).append(k == 3 || k == 6 ? "." : k == 9 ? "-" : "");
    StringBuilder mail = new StringBuilder();
    for (long k = 1, len = rand(8) + 3; k <= len; k++)
      mail.append("abcdefghijklmnopqrstuvwxyz0123456789".charAt((int) rand(36)));
    long kind = rand(20);
    String dom = new String[] {"mail.com", "example.com.br", "x.org"}[(int) rand(3)];
    if (kind == 0) mail.append(dom);
    else if (kind == 1) mail.append("@examplecom");
    else if (kind == 2) mail.append("!@").append(dom);
    else mail.append('@').append(dom);
    String date = String.format("%02d", rand(31) + 1) + "/" + String.format("%02d", rand(12) + 1) + "/"
        + (1990 + rand(41));
    StringBuilder amt = new StringBuilder();
    long len = rand(7) + 1;
    for (long k = 1; k <= len; k++) amt.append(rand(10));
    if (rand(10) == 0) {
      int p = (int) rand(len);
      amt.replace(p, p + 1, "x");
    }
    return new String[] {cpf.toString(), mail.toString(), date, amt.toString()};
  }

  public static void main(String[] args) {
    String[][] recs = new String[250000][];
    for (int i = 0; i < recs.length; i++) recs[i] = makeRecord();
    long t = System.nanoTime(), nc = 0, ne = 0, nd = 0, na = 0, all = 0, sum = 0;
    for (String[] r : recs) {
      boolean c = cpfOk(r[0]), e = emailOk(r[1]), d = dateOk(r[2]), a = amountOk(r[3]);
      if (a) sum += Long.parseLong(r[3]);
      nc += c ? 1 : 0;
      ne += e ? 1 : 0;
      nd += d ? 1 : 0;
      na += a ? 1 : 0;
      all += c && e && d && a ? 1 : 0;
    }
    boolean ok = nc == 187295 && ne == 212602 && nd == 245457 && na == 192603 && all == 120445
        && sum == 19490512431L;
    Bench.report("validacao_dados", t, ok);
  }
}
