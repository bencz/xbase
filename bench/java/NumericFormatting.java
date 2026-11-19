// bench/numeric_formatting.prg in Java: 80,000 numbers with three decimals (-50,000 to 50,000) and integers (0 to
// 999,999) from a fixed seed, each formatted with String.format (width and decimals, leading zeros, thousands
// separators, the separators exchanged as @E, a negative number in parentheses as @(, a blank for zero as @Z), a CPF
// mask inserted into a string as @R, rounded to one decimal with BigDecimal (half away from zero on the decimal
// text, as Round()), and read back with Double.parseDouble. The text's length, its separators and the values read
// back are checked.
import java.math.BigDecimal;
import java.math.RoundingMode;
import java.util.Locale;

public class NumericFormatting {
  static long seed = 314159;
  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  // The number in a field of `width` characters, as Str( v, width, dec ) or a picture of 9s gives it (`grouped` with
  // thousands separators): no sign when the digits shown are all zeros.
  static String num(double v, int width, int dec, boolean grouped) {
    String s = String.format(Locale.ROOT, grouped ? "%,." + dec + "f" : "%." + dec + "f", v);
    if (s.startsWith("-") && s.chars().allMatch(c -> c == '-' || c == '0' || c == '.' || c == ',')) s = s.substring(1);
    return " ".repeat(Math.max(0, width - s.length())) + s;
  }

  static double round(double v, int dec) {
    return BigDecimal.valueOf(v).setScale(dec, RoundingMode.HALF_UP).doubleValue();
  }

  // Transform( v, "@E 999,999.99" ): the point and the thousands separator exchanged.
  static String european(double v) {
    char[] c = num(v, 10, 2, true).toCharArray();
    for (int i = 0; i < c.length; i++) c[i] = c[i] == ',' ? '.' : c[i] == '.' ? ',' : c[i];
    return new String(c);
  }

  // Transform( v, "@( 99,999.99" ): a negative number's digits, its first character replaced by "(" and ")" after
  // (the digits as stars when the first one would be lost).
  static String parens(double v) {
    if (v >= 0) return num(v, 9, 2, true);
    char[] c = num(-v, 9, 2, true).toCharArray();
    if (c[0] >= '1' && c[0] <= '9')
      for (int i = 1; i < c.length; i++)
        if (Character.isDigit(c[i])) c[i] = '*';
    c[0] = '(';
    return new String(c) + ")";
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), cents = 0, tenths = 0;
    StringBuilder c = new StringBuilder();
    for (int i = 1; i <= 80000; i++) {
      long k = rand(100000000) - 50000000;
      if (Math.abs(k) % 10 == 5) k++;
      double x = k / 1000.0;
      long n = rand(1000000);
      String cpf = String.format("%011d", n);
      String str = num(x, 14, 2, false);
      c.append(str).append(String.format("%08d", n)).append(european(x)).append(num(x, 11, 3, true))
          .append(cpf, 0, 3).append('.').append(cpf, 3, 6).append('.').append(cpf, 6, 9).append('-')
          .append(cpf, 9, 11).append(parens(x)).append(n % 3 == 0 ? " " : Long.toString(n % 3))
          .append(num(round(x, 1), 12, 1, false).stripLeading()).append('\n');
      cents += (long) round(Double.parseDouble(str.strip()) * 100, 0);
      tenths += (long) round(round(x, 1) * 10, 0);
    }
    long comma = 0, dot = 0, paren = 0;
    for (int i = 0; i < c.length(); i++) {
      char ch = c.charAt(i);
      if (ch == ',') comma++;
      else if (ch == '.') dot++;
      else if (ch == '(') paren++;
    }
    boolean ok = c.length() == 6065015 && comma == 236856 && dot == 558428 && paren == 41318 && cents == -6496080269L
        && tenths == -649608444 && c.substring(0, 75).equals(
        "      35103.0200005610 35.103,02 35,103.019000.000.056-1035,103.02 35103.0" + "\n");
    Bench.report("numeric_formatting", t, ok);
  }
}
