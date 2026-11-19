// bench/url_encode.prg in Java: 20,000 strings of 20 to 80 bytes (letters, digits, blanks, punctuation and bytes above
// 127, as chars 0 .. 255) encoded as in a URL query (unreserved bytes kept, a blank as +, any other byte as %XX) and
// decoded back, written out; 8 rounds.
public class UrlEncode {
  static final String HEX_DIGITS = "0123456789ABCDEF";
  static final String UNRESERVED = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~";
  static long seed = 80;

  static long rand(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }

  static String urlEncode(String c) {
    StringBuilder out = new StringBuilder();
    for (int i = 0; i < c.length(); i++) {
      char x = c.charAt(i);
      if (UNRESERVED.indexOf(x) >= 0) {
        out.append(x);
      } else if (x == ' ') {
        out.append('+');
      } else {
        out.append('%').append(HEX_DIGITS.charAt(x / 16)).append(HEX_DIGITS.charAt(x % 16));
      }
    }
    return out.toString();
  }

  static String urlDecode(String c) {
    StringBuilder out = new StringBuilder();
    int i = 0, len = c.length();
    while (i < len) {
      char x = c.charAt(i);
      if (x == '%') {
        out.append((char) (HEX_DIGITS.indexOf(c.charAt(i + 1)) * 16 + HEX_DIGITS.indexOf(c.charAt(i + 2))));
        i += 3;
      } else {
        out.append(x == '+' ? ' ' : x);
        i++;
      }
    }
    return out.toString();
  }

  public static void main(String[] args) {
    String chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJ0123456789     -_.~&=/?%:#+" + (char) 233 + (char) 252;
    String[] a = new String[20000];
    for (int i = 0; i < a.length; i++) {
      StringBuilder c = new StringBuilder();
      for (long j = 1; j <= 20 + rand(61); j++) c.append(chars.charAt((int) rand(chars.length())));
      a[i] = c.toString();
    }
    long t = System.nanoTime(), n = 0;
    boolean ok = true;
    for (int r = 1; r <= 8; r++)
      for (String c : a) {
        String enc = urlEncode(c);
        n += enc.length() + enc.charAt(enc.length() - 1);
        ok = ok && urlDecode(enc).equals(c);
      }
    ok = ok && n == 18744664 && urlEncode("a b&c=" + (char) 233).equals("a+b%26c%3D%E9");
    Bench.report("url_encode", t, ok);
  }
}
