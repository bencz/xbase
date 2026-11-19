// bench/julia_set.prg in Java: the Julia set of c = -0.8 + 0.156i, escape times over a 1,600 x 1,200 grid of
// [-1.5, 1.5] x [-1, 1], at most 255 iterations per point, counted in a histogram of 256 bins folded into a checksum.
public class JuliaSet {
  public static void main(String[] args) {
    long t = System.nanoTime(), total = 0, cs = 0;
    int w = 1600, h = 1200, maxIter = 255;
    double cr = -0.8, ci = 0.156;
    long[] hist = new long[maxIter + 1];
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        double zr = x * 3.0 / w - 1.5, zi = y * 2.0 / h - 1.0;
        int k = 0;
        while (k < maxIter) {
          double zr2 = zr * zr, zi2 = zi * zi;
          if (zr2 + zi2 > 4.0) break;
          zi = 2.0 * zr * zi + ci;
          zr = zr2 - zi2 + cr;
          k++;
        }
        hist[k]++;
        total += k;
      }
    }
    for (long v : hist) cs = (cs * 31 + v) % 2147483648L;
    Bench.report("julia_set", t, total == 69243973 && cs == 2105097618);
  }
}
