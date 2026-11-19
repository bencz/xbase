// bench/md5_sha1.prg in Java: MD5 and SHA-1 (32-bit words in 64-bit integers, masked) of a 200,000-byte buffer from an
// LCG, 40 times, padded as the standards say; first checked against the published digests of "" and "abc".
import java.nio.charset.StandardCharsets;

public class Md5Sha1 {
  static final long MASK = 0xFFFFFFFFL;
  static final long[] MD5_K = {
    0xD76AA478L, 0xE8C7B756L, 0x242070DBL, 0xC1BDCEEEL, 0xF57C0FAFL, 0x4787C62AL, 0xA8304613L, 0xFD469501L,
    0x698098D8L, 0x8B44F7AFL, 0xFFFF5BB1L, 0x895CD7BEL, 0x6B901122L, 0xFD987193L, 0xA679438EL, 0x49B40821L,
    0xF61E2562L, 0xC040B340L, 0x265E5A51L, 0xE9B6C7AAL, 0xD62F105DL, 0x02441453L, 0xD8A1E681L, 0xE7D3FBC8L,
    0x21E1CDE6L, 0xC33707D6L, 0xF4D50D87L, 0x455A14EDL, 0xA9E3E905L, 0xFCEFA3F8L, 0x676F02D9L, 0x8D2A4C8AL,
    0xFFFA3942L, 0x8771F681L, 0x6D9D6122L, 0xFDE5380CL, 0xA4BEEA44L, 0x4BDECFA9L, 0xF6BB4B60L, 0xBEBFBC70L,
    0x289B7EC6L, 0xEAA127FAL, 0xD4EF3085L, 0x04881D05L, 0xD9D4D039L, 0xE6DB99E5L, 0x1FA27CF8L, 0xC4AC5665L,
    0xF4292244L, 0x432AFF97L, 0xAB9423A7L, 0xFC93A039L, 0x655B59C3L, 0x8F0CCC92L, 0xFFEFF47DL, 0x85845DD1L,
    0x6FA87E4FL, 0xFE2CE6E0L, 0xA3014314L, 0x4E0811A1L, 0xF7537E82L, 0xBD3AF235L, 0x2AD7D2BBL, 0xEB86D391L};
  static final int[] MD5_R = {7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21};

  static long rotl(long x, int s) { return ((x << s) | (x >> (32 - s))) & MASK; }

  // The words as lowercase hexadecimal, each word's bytes the least significant first when little.
  static String hex(long[] w, boolean little) {
    String digits = "0123456789abcdef";
    StringBuilder out = new StringBuilder();
    for (long x : w)
      for (int b = 0; b < 4; ++b) {
        int nByte = (int) ((x >> (little ? 8 * b : 24 - 8 * b)) & 0xFF);
        out.append(digits.charAt(nByte >> 4)).append(digits.charAt(nByte & 15));
      }
    return out.toString();
  }

  // The message, a 0x80 byte, zeros up to 56 bytes modulo 64, then its length in bits in 8 bytes.
  static byte[] pad(byte[] s, boolean little) {
    long n = s.length;
    int len = (int) ((n + 8) / 64 * 64 + 64);
    byte[] p = new byte[len];
    System.arraycopy(s, 0, p, 0, s.length);
    p[s.length] = (byte) 0x80;
    for (int i = 0; i < 8; ++i) p[len - 8 + (little ? i : 7 - i)] = (byte) (((n * 8) >> (8 * i)) & 0xFF);
    return p;
  }

  static long at(byte[] p, int i) { return p[i] & 0xFF; }

  static String md5(byte[] s) {
    long[] h = {0x67452301L, 0xEFCDAB89L, 0x98BADCFEL, 0x10325476L}, m = new long[16];
    byte[] p = pad(s, true);
    for (int blk = 0; blk < p.length; blk += 64) {
      for (int j = 0; j < 16; ++j) {
        int q = blk + 4 * j;
        m[j] = at(p, q) | (at(p, q + 1) << 8) | (at(p, q + 2) << 16) | (at(p, q + 3) << 24);
      }
      long a = h[0], b = h[1], c = h[2], d = h[3], f, tmp;
      int g;
      for (int i = 0; i < 64; ++i) {
        if (i < 16) {
          f = (b & c) | ((b ^ MASK) & d);
          g = i;
        } else if (i < 32) {
          f = (d & b) | ((d ^ MASK) & c);
          g = (5 * i + 1) & 15;
        } else if (i < 48) {
          f = b ^ c ^ d;
          g = (3 * i + 5) & 15;
        } else {
          f = c ^ (b | (d ^ MASK));
          g = (7 * i) & 15;
        }
        tmp = d;
        d = c;
        c = b;
        b = (b + rotl((a + f + MD5_K[i] + m[g]) & MASK, MD5_R[(i >> 4) * 4 + (i & 3)])) & MASK;
        a = tmp;
      }
      h[0] = (h[0] + a) & MASK;
      h[1] = (h[1] + b) & MASK;
      h[2] = (h[2] + c) & MASK;
      h[3] = (h[3] + d) & MASK;
    }
    return hex(h, true);
  }

  static String sha1(byte[] s) {
    long[] h = {0x67452301L, 0xEFCDAB89L, 0x98BADCFEL, 0x10325476L, 0xC3D2E1F0L}, w = new long[80];
    byte[] p = pad(s, false);
    for (int blk = 0; blk < p.length; blk += 64) {
      for (int j = 0; j < 16; ++j) {
        int q = blk + 4 * j;
        w[j] = (at(p, q) << 24) | (at(p, q + 1) << 16) | (at(p, q + 2) << 8) | at(p, q + 3);
      }
      for (int j = 16; j < 80; ++j) w[j] = rotl(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
      long a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f, k, tmp;
      for (int i = 0; i < 80; ++i) {
        if (i < 20) {
          f = (b & c) | ((b ^ MASK) & d);
          k = 0x5A827999L;
        } else if (i < 40) {
          f = b ^ c ^ d;
          k = 0x6ED9EBA1L;
        } else if (i < 60) {
          f = (b & c) | (b & d) | (c & d);
          k = 0x8F1BBCDCL;
        } else {
          f = b ^ c ^ d;
          k = 0xCA62C1D6L;
        }
        tmp = (rotl(a, 5) + f + e + k + w[i]) & MASK;
        e = d;
        d = c;
        c = rotl(b, 30);
        b = a;
        a = tmp;
      }
      h[0] = (h[0] + a) & MASK;
      h[1] = (h[1] + b) & MASK;
      h[2] = (h[2] + c) & MASK;
      h[3] = (h[3] + d) & MASK;
      h[4] = (h[4] + e) & MASK;
    }
    return hex(h, false);
  }

  static byte[] bytes(String s) { return s.getBytes(StandardCharsets.ISO_8859_1); }

  public static void main(String[] args) {
    long t = System.nanoTime(), seed = 7;
    int n = 200000;
    boolean ok = md5(bytes("")).equals("d41d8cd98f00b204e9800998ecf8427e")
        && md5(bytes("abc")).equals("900150983cd24fb0d6963f7d28e17f72")
        && sha1(bytes("")).equals("da39a3ee5e6b4b0d3255bfef95601890afd80709")
        && sha1(bytes("abc")).equals("a9993e364706816aba3e25717850c26c9cd0d89d");
    byte[] data = new byte[n];
    for (int i = 0; i < n; ++i) {
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      data[i] = (byte) ((seed >> 16) & 0xFF);
    }
    for (int rep = 1; rep <= 40; ++rep)
      ok = ok && md5(data).equals("9caadd7dbbe1174da261d335b92f85fb")
          && sha1(data).equals("cec3d79455d923d234a1a40741be135a4ba980e2");
    Bench.report("md5_sha1", t, ok);
  }
}
