// bench/base64.prg in Java: Base64 written out, 1,200,000 bytes from a fixed-seed generator (as the characters 0 to
// 255 of a String) encoded (three bytes to four characters through a table) and decoded back; 3 rounds.
public class Base64 {
  static final String TABLE = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static int[] values;
  static long seed = 64;
  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  static String encode(String c) {
    StringBuilder out = new StringBuilder();
    int len = c.length(), rest = len % 3;
    for (int i = 0; i < len - rest; i += 3) {
      int n = c.charAt(i) * 65536 + c.charAt(i + 1) * 256 + c.charAt(i + 2);
      out.append(TABLE.charAt(n / 262144)).append(TABLE.charAt(n / 4096 % 64)).append(TABLE.charAt(n / 64 % 64))
          .append(TABLE.charAt(n % 64));
    }
    if (rest == 1) {
      int n = c.charAt(len - 1);
      out.append(TABLE.charAt(n / 4)).append(TABLE.charAt((n % 4) * 16)).append("==");
    } else if (rest == 2) {
      int n = c.charAt(len - 2) * 256 + c.charAt(len - 1);
      out.append(TABLE.charAt(n / 1024)).append(TABLE.charAt(n / 16 % 64)).append(TABLE.charAt((n % 16) * 4))
          .append('=');
    }
    return out.toString();
  }

  static String decode(String c) {
    if (values == null) {
      values = new int[256];
      for (int i = 0; i < 64; i++) values[TABLE.charAt(i)] = i;
    }
    StringBuilder out = new StringBuilder();
    int len = c.length(), pad = c.endsWith("==") ? 2 : c.endsWith("=") ? 1 : 0;
    for (int i = 0; i < len; i += 4) {
      int n = values[c.charAt(i)] * 262144 + values[c.charAt(i + 1)] * 4096 + values[c.charAt(i + 2)] * 64
          + values[c.charAt(i + 3)];
      out.append((char) (n / 65536)).append((char) (n / 256 % 256)).append((char) (n % 256));
    }
    return out.substring(0, out.length() - pad);
  }

  public static void main(String[] args) {
    StringBuilder b = new StringBuilder();
    for (int i = 1; i <= 1200000; i++) b.append((char) rand(256));
    String c = b.toString();
    long t = System.nanoTime(), n = 0;
    boolean ok = true;
    for (int r = 1; r <= 3; r++) {
      String enc = encode(c.substring(0, c.length() - r + 1)), dec = decode(enc);
      ok = ok && dec.equals(c.substring(0, c.length() - r + 1));
      n += enc.length();
      for (int i = 0; i < enc.length(); i += 97) n += enc.charAt(i);
    }
    ok = ok && n == 9038319 && encode("Ma").equals("TWE=") && decode("TQ==").equals("M");
    Bench.report("base64", t, ok);
  }
}
