// bench/mandelbrot.prg in Java: the Mandelbrot set as a bitmap, 1,600 x 1,600 points, at most 50 iterations each,
// one bit per point packed into bytes (a row padded to a whole byte), the bytes folded into a checksum.
public class Mandelbrot {
  public static void main(String[] args) {
    long t = System.nanoTime(), cs = 0, nBytes = 0;
    int n = 1600;
    for (int y = 0; y < n; y++) {
      double ci = 2.0 * y / n - 1.0;
      long bits = 0;
      int nBits = 0;
      for (int x = 0; x < n; x++) {
        double cr = 2.0 * x / n - 1.5, zr = 0.0, zi = 0.0, tr = 0.0, ti = 0.0;
        int k = 0;
        while (k < 50 && tr + ti <= 4.0) {
          zi = 2.0 * zr * zi + ci;
          zr = tr - ti + cr;
          tr = zr * zr;
          ti = zi * zi;
          k++;
        }
        bits = (bits << 1) | (tr + ti <= 4.0 ? 1 : 0);
        nBits++;
        if (nBits == 8 || x == n - 1) {
          bits = bits << (8 - nBits);
          cs = (cs * 31 + bits) & 0x7FFFFFFF;
          nBytes++;
          bits = 0;
          nBits = 0;
        }
      }
    }
    Bench.report("mandelbrot", t, cs == 366763831 && nBytes == n * ((n + 7) / 8));
  }
}
