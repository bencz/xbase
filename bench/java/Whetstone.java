// bench/whetstone.prg in Java: Whetstone's modules 2, 3, 4, 6, 8, 9 and 11 scaled by a loop count of 5,000 (module
// 7, the trigonometric one, is left out as in the .prg); the check is the sum of the values each module leaves.
// Arrays are 1-based as in the .prg (element 0 unused); P3's parameter by reference is a one-element array.
public class Whetstone {
  static final double T = 0.499975, T1 = 0.50025, T2 = 2.0;
  static final double[] E1 = new double[5];
  static long j, k, l;

  static void pa(double[] e) {
    for (int n = 0; n < 6; n++) {
      e[1] = (e[1] + e[2] + e[3] - e[4]) * T;
      e[2] = (e[1] + e[2] - e[3] + e[4]) * T;
      e[3] = (e[1] - e[2] + e[3] + e[4]) * T;
      e[4] = (-e[1] + e[2] + e[3] + e[4]) / T2;
    }
  }

  static void p0() {
    E1[(int) j] = E1[(int) k];
    E1[(int) k] = E1[(int) l];
    E1[(int) l] = E1[(int) j];
  }

  static void p3(double x, double y, double[] z) {
    double x1 = x, y1 = y;
    x1 = T * (x1 + y1);
    y1 = T * (x1 + y1);
    z[0] = (x1 + y1) / T2;
  }

  public static void main(String[] args) {
    long tm = System.nanoTime(), nLoop = 5000;
    double[] e1 = E1;
    e1[1] = 1.0;
    e1[2] = -1.0;
    e1[3] = -1.0;
    e1[4] = -1.0;
    for (long i = 1; i <= 12 * nLoop; i++) {
      e1[1] = (e1[1] + e1[2] + e1[3] - e1[4]) * T;
      e1[2] = (e1[1] + e1[2] - e1[3] + e1[4]) * T;
      e1[3] = (e1[1] - e1[2] + e1[3] + e1[4]) * T;
      e1[4] = (-e1[1] + e1[2] + e1[3] + e1[4]) * T;
    }
    double check = e1[1] + e1[2] + e1[3] + e1[4];
    for (long i = 1; i <= 14 * nLoop; i++) pa(e1);
    check += e1[1] + e1[2] + e1[3] + e1[4];
    j = 1;
    for (long i = 1; i <= 345 * nLoop; i++) {
      j = j == 1 ? 2 : 3;
      j = j > 2 ? 0 : 1;
      j = j < 1 ? 1 : 0;
    }
    check += j;
    j = 1;
    k = 2;
    l = 3;
    for (long i = 1; i <= 210 * nLoop; i++) {
      j = j * (k - j) * (l - k);
      k = l * k - (l - j) * k;
      l = (l - k) * (k + j);
      e1[(int) (l - 1)] = j + k + l;
      e1[(int) (k - 1)] = j * k * l;
    }
    check += j + k + l + e1[1] + e1[2] + e1[3] + e1[4];
    double x = 1.0, y = 1.0;
    double[] z = {1.0};
    for (long i = 1; i <= 899 * nLoop; i++) p3(x, y, z);
    check += x + y + z[0];
    j = 1;
    k = 2;
    l = 3;
    e1[1] = 1.0;
    e1[2] = 2.0;
    e1[3] = 3.0;
    for (long i = 1; i <= 616 * nLoop; i++) p0();
    check += e1[1] + e1[2] + e1[3] + e1[4];
    x = 0.75;
    for (long i = 1; i <= 93 * nLoop; i++) x = Math.sqrt(Math.exp(Math.log(x) / T1));
    check += x;
    Bench.report("whetstone", tm, Math.abs(check - 30.95582672351981) < 0.000000001);
  }
}
