// bench/long_short_mix.prg in Java: long-lived and short-lived values together: 100,000 records that live to the end,
// 2,500,000 temporary records and strings, one in 8 of them stored into a long-lived record (an old value pointing to
// a new one), one record in 64 steps replaced by a new one.
public class LongShortMix {
  static final class Rec {
    long num;
    String text;
    Rec link;

    Rec(long num, String text, Rec link) {
      this.num = num;
      this.text = text;
      this.link = link;
    }
  }

  static long seed = 17;

  static long rand(long n) {
    seed = (seed * 16807) % 2147483647;
    return seed % n;
  }

  // Str( n ): a number right-aligned in 10 characters.
  static String str(long n) {
    return String.format("%10d", n);
  }

  public static void main(String[] args) {
    long t = System.nanoTime(), n = 0;
    Rec[] aLong = new Rec[100000];
    for (int i = 1; i <= aLong.length; i++) aLong[i - 1] = new Rec(i, str(i), null);
    for (long i = 1; i <= 2500000; i++) {
      Rec x = new Rec(i % 1000, String.valueOf((char) (97 + i % 26)).repeat((int) (i % 40 + 1)), null);
      n += x.text.length();
      if (i % 8 == 0) {
        int k = (int) rand(aLong.length);
        aLong[k].link = x;
      }
      if (i % 64 == 0) {
        int k = (int) rand(aLong.length);
        aLong[k] = new Rec(aLong[k].num + 1, str(i), aLong[k].link);
      }
    }
    for (Rec r : aLong) {
      n += r.num + r.text.length();
      if (r.link != null) n += r.link.num;
    }
    Bench.report("long_short_mix", t, n == 5099739142L);
  }
}
