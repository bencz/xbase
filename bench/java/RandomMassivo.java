// bench/random_massivo.prg in Java: 30,000,000 rounds of four random-number generators in integer arithmetic (a
// 31-bit LCG, a 64-bit xorshift, multiply-with-carry, a 32-bit xorshift), their sums, a histogram of their top bytes
// and the LCG's numbers scaled to [0, 1) summed.
public class RandomMassivo {
  public static void main(String[] args) {
    long t = System.nanoTime(), lcg = 12345, xs64 = 88172645463325252L, z = 362436069, w = 521288629;
    long xs32 = 2463534242L, s1 = 0, s2 = 0, s3 = 0, s4 = 0, check = 0, r;
    long[] hist = new long[256];
    double fsum = 0.0;
    for (int i = 1; i <= 30000000; i++) {
      lcg = (lcg * 1103515245 + 12345) & 0x7FFFFFFF;
      s1 += lcg;
      hist[(int) (lcg >> 23)]++;
      fsum += lcg / 2147483648.0;

      xs64 = xs64 ^ (xs64 << 13);
      xs64 = xs64 ^ ((xs64 >> 7) & 0x01FFFFFFFFFFFFFFL);
      xs64 = xs64 ^ (xs64 << 17);
      r = (xs64 >> 32) & 0xFFFFFFFFL;
      s2 += r;
      hist[(int) (r >> 24)]++;

      z = 36969 * (z & 0xFFFF) + (z >> 16);
      w = 18000 * (w & 0xFFFF) + (w >> 16);
      r = ((z << 16) + w) & 0xFFFFFFFFL;
      s3 += r;
      hist[(int) (r >> 24)]++;

      xs32 = xs32 ^ ((xs32 << 13) & 0xFFFFFFFFL);
      xs32 = xs32 ^ (xs32 >> 17);
      xs32 = xs32 ^ ((xs32 << 5) & 0xFFFFFFFFL);
      s4 += xs32;
      hist[(int) (xs32 >> 24)]++;
    }
    for (int i = 1; i <= 256; i++) check += hist[i - 1] * i;
    boolean ok = s1 == 32217797789519680L && s2 == 64428429046975605L && s3 == 64423232821249815L
        && s4 == 64428119329011698L && check == 15421035017L && Math.abs(fsum - 15002581.187856009) < 0.000001;
    Bench.report("random_massivo", t, ok);
  }
}
