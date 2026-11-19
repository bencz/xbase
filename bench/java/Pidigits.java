// bench/pidigits.prg in Java: the first 1,500 digits of pi from Gibbons' unbounded spigot, its numbers kept as arrays
// of 30-bit limbs (the least significant first; the accumulator's sign apart), with multiplication by a small number,
// addition and subtraction of a multiple, comparison, and a quotient below 2^30 estimated from the leading limbs with
// doubles, then corrected; the copies made where the .prg clones. Checked: the first and the last 50 digits and their
// sum.
import java.util.Arrays;

public class Pidigits {
  static final int BITS = 30;
  static final long LIMB_MASK = 0x3FFFFFFF;
  static final double RADIX = 1073741824.0;

  // A number: its limbs d[0 .. n - 1], the least significant first.
  static final class Big {
    long[] d;
    int n;

    Big(long v) {
      d = new long[] {v};
      n = 1;
    }

    Big(Big b) {
      d = Arrays.copyOf(b.d, b.n);
      n = b.n;
    }

    void push(long v) {
      if (n == d.length) d = Arrays.copyOf(d, n * 2);
      d[n++] = v;
    }

    void resize(int m) {
      if (m > d.length) d = Arrays.copyOf(d, m);
      for (int i = n; i < m; i++) d[i] = 0;
      n = m;
    }

    // Drops the leading zero limbs (keeps one).
    void normalize() {
      while (n > 1 && d[n - 1] == 0) n--;
    }
  }

  // a *= m
  static void mulSmall(Big a, long m) {
    long carry = 0;
    for (int i = 0; i < a.n; i++) {
      long x = a.d[i] * m + carry;
      a.d[i] = x & LIMB_MASK;
      carry = x >> BITS;
    }
    while (carry > 0) {
      a.push(carry & LIMB_MASK);
      carry >>= BITS;
    }
    a.normalize();
  }

  // a += b * m
  static void addMul(Big a, Big b, long m) {
    long carry = 0;
    int i;
    if (a.n < b.n) a.resize(b.n);
    for (i = 0; i < b.n; i++) {
      long x = a.d[i] + b.d[i] * m + carry;
      a.d[i] = x & LIMB_MASK;
      carry = x >> BITS;
    }
    for (; carry > 0 && i < a.n; i++) {
      long x = a.d[i] + carry;
      a.d[i] = x & LIMB_MASK;
      carry = x >> BITS;
    }
    while (carry > 0) {
      a.push(carry & LIMB_MASK);
      carry >>= BITS;
    }
  }

  // a -= b * m, when b * m <= a
  static void subMul(Big a, Big b, long m) {
    long borrow = 0;
    int i;
    for (i = 0; i < b.n; i++) {
      long x = a.d[i] - b.d[i] * m - borrow;
      a.d[i] = x & LIMB_MASK;
      borrow = -(x >> BITS);
    }
    for (; borrow > 0; i++) {
      long x = a.d[i] - borrow;
      a.d[i] = x & LIMB_MASK;
      borrow = -(x >> BITS);
    }
    a.normalize();
  }

  // a += b * m (m >= 0), neg[0] the sign of a, termNeg that of the term.
  static void addSigned(Big a, boolean[] neg, Big b, long m, boolean termNeg) {
    if (neg[0] == termNeg) {
      addMul(a, b, m);
    } else {
      Big tmp = new Big(b);
      mulSmall(tmp, m);
      if (cmp(a, tmp) >= 0) {
        subMul(a, tmp, 1);
      } else {
        subMul(tmp, a, 1);
        a.d = Arrays.copyOf(tmp.d, tmp.n);
        a.n = tmp.n;
        neg[0] = termNeg;
      }
    }
    if (a.n == 1 && a.d[0] == 0) neg[0] = false;
  }

  static int cmp(Big a, Big b) {
    if (a.n != b.n) return a.n < b.n ? -1 : 1;
    for (int i = a.n - 1; i >= 0; i--)
      if (a.d[i] != b.d[i]) return a.d[i] < b.d[i] ? -1 : 1;
    return 0;
  }

  // The limb at i as a double, 0 past either end.
  static double top(Big a, int i) { return i >= 0 && i < a.n ? a.d[i] * 1.0 : 0.0; }

  // (long) (x / d) for a quotient below 2^30: estimated from the three leading limbs, then corrected.
  static long quotient(Big x, Big d) {
    int h = Math.max(x.n, d.n) - 1;
    double fx = (top(x, h) * RADIX + top(x, h - 1)) * RADIX + top(x, h - 2);
    double fd = (top(d, h) * RADIX + top(d, h - 1)) * RADIX + top(d, h - 2);
    long q = (long) (fx / fd);
    Big tmp = new Big(d);
    mulSmall(tmp, q);
    while (cmp(tmp, x) > 0) {
      subMul(tmp, d, 1);
      q--;
    }
    addMul(tmp, d, 1);
    while (cmp(tmp, x) <= 0) {
      addMul(tmp, d, 1);
      q++;
    }
    return q;
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), k = 0, sum = 0;
    int n = 1500;
    StringBuilder digits = new StringBuilder();
    boolean[] neg = {false}, xNeg = {false};
    Big numer = new Big(1), accum = new Big(0), denom = new Big(1);
    while (digits.length() < n) {
      k++;
      addSigned(accum, neg, numer, 2, false);
      mulSmall(accum, 2 * k + 1);
      mulSmall(denom, 2 * k + 1);
      mulSmall(numer, k);
      if (neg[0] || cmp(numer, accum) > 0) continue;
      Big x = new Big(accum);
      xNeg[0] = neg[0];
      addSigned(x, xNeg, numer, 3, false);
      long d = quotient(x, denom);
      addSigned(x, xNeg, numer, 1, false);
      if (d != quotient(x, denom)) continue;
      digits.append((char) (48 + d));
      addSigned(accum, neg, denom, d, true);
      mulSmall(accum, 10);
      mulSmall(numer, 10);
    }
    for (int i = 0; i < n; i++) sum += digits.charAt(i) - 48;
    String s = digits.toString();
    boolean ok = s.startsWith("31415926535897932384626433832795028841971693993751")
        && s.endsWith("55570674983850549458858692699569092721079750930295") && sum == 6775;
    Bench.report("pidigits", t, ok);
  }
}
