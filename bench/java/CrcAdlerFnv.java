// bench/crc_adler_fnv.prg in Java: checksums of bytes: twenty strings of 1,000,000 characters from an LCG, built a
// character at a time in a StringBuilder, each hashed with CRC-32 (from a table), Adler-32 and FNV-1a (32 bits); the
// functions are first checked against their published values ("123456789", "Wikipedia", "a").
public class CrcAdlerFnv {
  static long crc32Of(long[] table, String s) {
    long crc = 0xFFFFFFFFL;
    for (int i = 0; i < s.length(); i++) crc = table[(int) ((crc ^ s.charAt(i)) & 0xFF)] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFL;
  }

  static long adler32Of(String s) {
    long a = 1, b = 0;
    for (int i = 0; i < s.length(); i++) {
      a += s.charAt(i);
      if (a >= 65521) a -= 65521;
      b += a;
      if (b >= 65521) b -= 65521;
    }
    return (b << 16) | a;
  }

  static long fnv1aOf(String s) {
    long h = 2166136261L;
    for (int i = 0; i < s.length(); i++) h = ((h ^ s.charAt(i)) * 16777619) & 0xFFFFFFFFL;
    return h;
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), seed = 42, total = 0;
    int n = 1000000;
    long[] table = new long[256];
    for (int i = 0; i <= 255; i++) {
      long c = i;
      for (int k = 1; k <= 8; k++) c = (c >> 1) ^ (0xEDB88320L & -(c & 1));
      table[i] = c;
    }
    boolean ok = crc32Of(table, "123456789") == 0xCBF43926L && adler32Of("Wikipedia") == 0x11E60398L
        && fnv1aOf("a") == 0xE40C292CL;
    for (int rep = 1; rep <= 20; rep++) {
      StringBuilder data = new StringBuilder();
      for (int i = 1; i <= n; i++) {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        data.append((char) ((seed >> 16) & 0xFF));
      }
      String s = data.toString();
      total += crc32Of(table, s) + adler32Of(s) + fnv1aOf(s);
    }
    Bench.report("crc_adler_fnv", t, ok && total == 142519858956L);
  }
}
