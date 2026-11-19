// bench/n_body, spectral_norm, fft, linpack, dgemm, whetstone, dhrystone, ray_tracer, black_scholes, julia_set,
// mandelbrot, monte_carlo_pi, numerical_integration, gaussian_elimination, pidigits and random_massivo .prg in C#: the
// same work and checks, the numbers in arrays of doubles or longs as the C ports hold them.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Text;

static class NBodyBench {
  static double Energy(double[] x, double[] y, double[] z, double[] vx, double[] vy, double[] vz, double[] m) {
    double e = 0.0;
    for (int i = 0; i < m.Length; i++) {
      e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
      for (int j = i + 1; j < m.Length; j++) {
        double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
        e -= m[i] * m[j] / Math.Sqrt(dx * dx + dy * dy + dz * dz);
      }
    }
    return e;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int steps = 500000, nb = 5;
    double pi = 3.141592653589793, days = 365.24, dt = 0.01, px = 0.0, py = 0.0, pz = 0.0;
    double solarMass = 4.0 * pi * pi;
    double[] x = { 0.0, 4.84143144246472090, 8.34336671824457987, 12.8943695621391310, 15.3796971148509165 };
    double[] y = { 0.0, -1.16032004402742839, 4.12479856412430479, -15.1111514016986312, -25.9193146099879641 };
    double[] z = { 0.0, -0.103622044471123109, -0.403523417114321381, -0.223307578892655734, 0.179258772950371181 };
    double[] vx = { 0.0, 0.00166007664274403694 * days, -0.00276742510726862411 * days,
                    0.00296460137564761618 * days, 0.00268067772490389322 * days };
    double[] vy = { 0.0, 0.00769901118419740425 * days, 0.00499852801234917238 * days,
                    0.00237847173959480950 * days, 0.00162824170038242295 * days };
    double[] vz = { 0.0, -0.0000690460016972063023 * days, 0.0000230417297573763929 * days,
                    -0.0000296589568540237556 * days, -0.0000951592254519715870 * days };
    double[] m = { solarMass, 0.000954791938424326609 * solarMass, 0.000285885980666130812 * solarMass,
                   0.0000436624404335156298 * solarMass, 0.0000515138902046611451 * solarMass };
    for (int i = 0; i < nb; i++) {
      px += vx[i] * m[i];
      py += vy[i] * m[i];
      pz += vz[i] * m[i];
    }
    vx[0] = -px / solarMass;
    vy[0] = -py / solarMass;
    vz[0] = -pz / solarMass;
    double e0 = Energy(x, y, z, vx, vy, vz, m);
    for (int k = 1; k <= steps; k++) {
      for (int i = 0; i < nb; i++) {
        double mi = m[i];
        for (int j = i + 1; j < nb; j++) {
          double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
          double d2 = dx * dx + dy * dy + dz * dz;
          double mag = dt / (d2 * Math.Sqrt(d2));
          double mj = m[j];
          vx[i] -= dx * mj * mag;
          vy[i] -= dy * mj * mag;
          vz[i] -= dz * mj * mag;
          vx[j] += dx * mi * mag;
          vy[j] += dy * mi * mag;
          vz[j] += dz * mi * mag;
        }
      }
      for (int i = 0; i < nb; i++) {
        x[i] += dt * vx[i];
        y[i] += dt * vy[i];
        z[i] += dt * vz[i];
      }
    }
    double e1 = Energy(x, y, z, vx, vy, vz, m);
    Program.Report("n_body", sw, Math.Abs(e0 + 0.169075164) < 0.000000001 && Math.Abs(e1 + 0.169096567) < 0.000000001);
  }
}

static class SpectralNormBench {
  // Each element of A from a call, as the .prg's EvalA().
  [MethodImpl(MethodImplOptions.NoInlining)]
  static double EvalA(long i, long j) => 1.0 / ((i + j) * (i + j + 1) / 2 + i + 1);

  static void MulAv(int n, double[] u, double[] v) {
    for (int i = 0; i < n; i++) {
      double s = 0.0;
      for (int j = 0; j < n; j++) s += EvalA(i, j) * u[j];
      v[i] = s;
    }
  }

  static void MulAtv(int n, double[] u, double[] v) {
    for (int i = 0; i < n; i++) {
      double s = 0.0;
      for (int j = 0; j < n; j++) s += EvalA(j, i) * u[j];
      v[i] = s;
    }
  }

  static void MulAtAv(int n, double[] u, double[] v, double[] w) {
    MulAv(n, u, w);
    MulAtv(n, w, v);
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 1200;
    var u = new double[n];
    var v = new double[n];
    var w = new double[n];
    double vbv = 0.0, vv = 0.0;
    Array.Fill(u, 1.0);
    for (int i = 1; i <= 10; i++) {
      MulAtAv(n, u, v, w);
      MulAtAv(n, v, u, w);
    }
    for (int i = 0; i < n; i++) {
      vbv += u[i] * v[i];
      vv += v[i] * v[i];
    }
    double r = Math.Sqrt(vbv / vv);
    Program.Report("spectral_norm", sw, Math.Abs(r - 1.2742241501) < 0.000000001);
  }
}

static class FftBench {
  static void Fft(double[] re, double[] im, int n, double[] cosT, double[] sinT, double sign) {
    int j = 0;
    for (int i = 0; i <= n - 2; i++) {
      if (i < j) {
        (re[i], re[j]) = (re[j], re[i]);
        (im[i], im[j]) = (im[j], im[i]);
      }
      int k = n >> 1;
      while (k <= j) {
        j -= k;
        k >>= 1;
      }
      j += k;
    }
    for (int len = 2, st = 0; len <= n; len <<= 1, st++) {
      int half = len >> 1;
      double wr0 = cosT[st], wi0 = sign * sinT[st];
      for (int i = 0; i < n; i += len) {
        double wr = 1.0, wi = 0.0;
        for (int jj = i; jj <= i + half - 1; jj++) {
          int k = jj + half;
          double xr = re[k] * wr - im[k] * wi;
          double xi = re[k] * wi + im[k] * wr;
          re[k] = re[jj] - xr;
          im[k] = im[jj] - xi;
          re[jj] += xr;
          im[jj] += xi;
          double tmp = wr * wr0 - wi * wi0;
          wi = wr * wi0 + wi * wr0;
          wr = tmp;
        }
      }
    }
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int logN = 16, n = 1 << logN;
    long seed = 2024;
    var re = new double[n];
    var im = new double[n];
    double spec = 0.0, maxErr = 0.0;
    for (int i = 0; i < n; i++) {
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      re[i] = seed / 1073741824.0 - 1.0;
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      im[i] = seed / 1073741824.0 - 1.0;
    }
    var re0 = (double[])re.Clone();
    var im0 = (double[])im.Clone();
    // cos and sin of 2 pi / 2^k, for k from 1
    var cosT = new double[logN];
    var sinT = new double[logN];
    for (int i = 0; i < logN; i++) {
      cosT[i] = Math.Cos(2.0 * Math.PI / (1L << (i + 1)));
      sinT[i] = Math.Sin(2.0 * Math.PI / (1L << (i + 1)));
    }
    for (int rep = 1; rep <= 10; rep++) {
      Fft(re, im, n, cosT, sinT, -1.0);
      if (rep == 1)
        for (int i = 0; i < n; i++) spec += Math.Abs(re[i]) + Math.Abs(im[i]);
      Fft(re, im, n, cosT, sinT, 1.0);
      for (int i = 0; i < n; i++) {
        re[i] = re[i] / n;
        im[i] = im[i] / n;
      }
    }
    for (int i = 0; i < n; i++)
      maxErr = Math.Max(maxErr, Math.Max(Math.Abs(re[i] - re0[i]), Math.Abs(im[i] - im0[i])));
    Program.Report("fft", sw, Math.Abs(spec / 15471706.35354992 - 1) < 0.000000001 && maxErr < 0.000000001);
  }
}

// Indices 1-based as in the .prg (element 0 unused).
static class LinpackBench {
  static void MatGen(double[] a, double[] b, int n) {
    long init = 1325;
    for (int j = 1; j <= n; j++)
      for (int i = 1; i <= n; i++) {
        init = (3125 * init) % 65536;
        a[(j - 1) * n + i] = (init - 32768.0) / 16384.0;
      }
    Array.Fill(b, 0.0);
    for (int j = 1; j <= n; j++)
      for (int i = 1; i <= n; i++) b[i] += a[(j - 1) * n + i];
  }

  static void DAxpy(int n, double da, double[] dx, int x, double[] dy, int y) {
    if (n > 0 && da != 0.0)
      for (int i = 0; i <= n - 1; i++) dy[y + i] += da * dx[x + i];
  }

  static void DScal(int n, double da, double[] dx, int x) {
    for (int i = x; i <= x + n - 1; i++) dx[i] = da * dx[i];
  }

  static int IDAMax(int n, double[] dx, int x) {
    int imax = 1;
    double dmax = Math.Abs(dx[x]);
    for (int i = 2; i <= n; i++) {
      double v = Math.Abs(dx[x + i - 1]);
      if (v > dmax) {
        imax = i;
        dmax = v;
      }
    }
    return imax;
  }

  static void DGeFa(double[] a, int n, int[] ipvt) {
    for (int k = 1; k <= n - 1; k++) {
      int col = (k - 1) * n;
      int l = IDAMax(n - k + 1, a, col + k) + k - 1;
      ipvt[k] = l;
      if (a[col + l] != 0.0) {
        double t;
        if (l != k) {
          t = a[col + l];
          a[col + l] = a[col + k];
          a[col + k] = t;
        }
        t = -1.0 / a[col + k];
        DScal(n - k, t, a, col + k + 1);
        for (int j = k + 1; j <= n; j++) {
          t = a[(j - 1) * n + l];
          if (l != k) {
            a[(j - 1) * n + l] = a[(j - 1) * n + k];
            a[(j - 1) * n + k] = t;
          }
          DAxpy(n - k, t, a, col + k + 1, a, (j - 1) * n + k + 1);
        }
      }
    }
    ipvt[n] = n;
  }

  static void DGeSl(double[] a, int n, int[] ipvt, double[] b) {
    for (int k = 1; k <= n - 1; k++) {
      int l = ipvt[k];
      double t = b[l];
      if (l != k) {
        b[l] = b[k];
        b[k] = t;
      }
      DAxpy(n - k, t, a, (k - 1) * n + k + 1, b, k + 1);
    }
    for (int k = n; k >= 1; k--) {
      b[k] = b[k] / a[(k - 1) * n + k];
      double t = -b[k];
      DAxpy(k - 1, t, a, (k - 1) * n + 1, b, 1);
    }
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 300;
    bool ok = true;
    for (int rep = 1; rep <= 5; rep++) {
      var a = new double[n * n + 1];
      var b = new double[n + 1];
      var ipvt = new int[n + 1];
      MatGen(a, b, n);
      var x = (double[])b.Clone();
      DGeFa(a, n, ipvt);
      DGeSl(a, n, ipvt, x);
      // the factorization overwrote a: a new copy gives the residual
      MatGen(a, b, n);
      double maxErr = 0.0, resid = 0.0;
      for (int i = 1; i <= n; i++) {
        maxErr = Math.Max(maxErr, Math.Abs(x[i] - 1.0));
        double s = -b[i];
        for (int j = 1; j <= n; j++) s += a[(j - 1) * n + i] * x[j];
        resid = Math.Max(resid, Math.Abs(s));
      }
      ok = ok && maxErr < 0.000000001 && resid < 0.000000001;
    }
    Program.Report("linpack", sw, ok);
  }
}

static class DgemmBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 250;
    long seed = 7;
    double alpha = 1.5, beta = 0.5, sum = 0.0, trace = 0.0;
    var a = new double[n * n];
    var b = new double[n * n];
    var c = new double[n * n];
    for (int i = 0; i < n * n; i++) {
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      a[i] = seed / 1073741824.0 - 1.0;
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      b[i] = seed / 1073741824.0 - 1.0;
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      c[i] = seed / 1073741824.0 - 1.0;
    }
    for (int rep = 1; rep <= 3; rep++)
      for (int i = 0; i < n; i++) {
        int ci = i * n;
        for (int j = ci; j < ci + n; j++) c[j] = beta * c[j];
        for (int k = 0; k < n; k++) {
          double aik = alpha * a[ci + k];
          int bk = k * n - ci;
          for (int j = ci; j < ci + n; j++) c[j] += aik * b[bk + j];
        }
      }
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) sum += c[i * n + j];
      trace += c[i * n + i];
    }
    Program.Report("dgemm", sw,
                   Math.Abs(sum + 145.62810535074) < 0.000001 && Math.Abs(trace - 342.80906789597) < 0.000001);
  }
}

// Arrays 1-based as in the .prg (element 0 unused).
static class WhetstoneBench {
  static double t = 0.499975, t1 = 0.50025, t2 = 2.0;
  static readonly double[] e1 = new double[5];
  static long j, k, l;

  [MethodImpl(MethodImplOptions.NoInlining)]
  static void PA(double[] e) {
    for (int n = 0; n < 6; n++) {
      e[1] = (e[1] + e[2] + e[3] - e[4]) * t;
      e[2] = (e[1] + e[2] - e[3] + e[4]) * t;
      e[3] = (e[1] - e[2] + e[3] + e[4]) * t;
      e[4] = (-e[1] + e[2] + e[3] + e[4]) / t2;
    }
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static void P0() {
    e1[j] = e1[k];
    e1[k] = e1[l];
    e1[l] = e1[j];
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static void P3(double x, double y, ref double z) {
    double x1 = x, y1 = y;
    x1 = t * (x1 + y1);
    y1 = t * (x1 + y1);
    z = (x1 + y1) / t2;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    long nLoop = 5000;
    double x, y, z, check;
    e1[1] = 1.0;
    e1[2] = -1.0;
    e1[3] = -1.0;
    e1[4] = -1.0;
    for (long i = 1; i <= 12 * nLoop; i++) {
      e1[1] = (e1[1] + e1[2] + e1[3] - e1[4]) * t;
      e1[2] = (e1[1] + e1[2] - e1[3] + e1[4]) * t;
      e1[3] = (e1[1] - e1[2] + e1[3] + e1[4]) * t;
      e1[4] = (-e1[1] + e1[2] + e1[3] + e1[4]) * t;
    }
    check = e1[1] + e1[2] + e1[3] + e1[4];
    for (long i = 1; i <= 14 * nLoop; i++) PA(e1);
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
      e1[l - 1] = j + k + l;
      e1[k - 1] = j * k * l;
    }
    check += j + k + l + e1[1] + e1[2] + e1[3] + e1[4];
    x = 0.5;
    y = 0.5;
    for (long i = 1; i <= 32 * nLoop; i++) {
      x = t * Math.Atan(t2 * Math.Sin(x) * Math.Cos(x) / (Math.Cos(x + y) + Math.Cos(x - y) - 1.0));
      y = t * Math.Atan(t2 * Math.Sin(y) * Math.Cos(y) / (Math.Cos(x + y) + Math.Cos(x - y) - 1.0));
    }
    check += x + y;
    x = 1.0;
    y = 1.0;
    z = 1.0;
    for (long i = 1; i <= 899 * nLoop; i++) P3(x, y, ref z);
    check += x + y + z;
    j = 1;
    k = 2;
    l = 3;
    e1[1] = 1.0;
    e1[2] = 2.0;
    e1[3] = 3.0;
    for (long i = 1; i <= 616 * nLoop; i++) P0();
    check += e1[1] + e1[2] + e1[3] + e1[4];
    x = 0.75;
    for (long i = 1; i <= 93 * nLoop; i++) x = Math.Sqrt(Math.Exp(Math.Log(x) / t1));
    check += x;
    Program.Report("whetstone", sw, Math.Abs(check - 30.95612841002271) < 0.000000001);
  }
}

static class DhrystoneBench {
  const int Ident1 = 0, Ident2 = 1, Ident3 = 2, Ident4 = 3, Ident5 = 4, Runs = 1000000;

  sealed class Record {
    public Record PtrComp;
    public int Discr, EnumComp;
    public long IntComp;
    public string StrComp;
    public void CopyFrom(Record r) {
      PtrComp = r.PtrComp;
      Discr = r.Discr;
      EnumComp = r.EnumComp;
      IntComp = r.IntComp;
      StrComp = r.StrComp;
    }
  }

  static long intGlob;
  static bool boolGlob;
  static char ch1Glob = ' ', ch2Glob = ' ';
  static readonly long[] arr1Glob = new long[50];
  static readonly long[][] arr2Glob = MakeArr2();
  static Record ptrGlob, nextPtrGlob;

  static long[][] MakeArr2() {
    var a = new long[50][];
    for (int i = 0; i < a.Length; i++) a[i] = new long[50];
    return a;
  }

  // The procedures are calls, as xbase makes them.
  [MethodImpl(MethodImplOptions.NoInlining)]
  static void Proc1(Record ptrValPar) {
    Record nextRecord = ptrValPar.PtrComp;
    ptrValPar.PtrComp.CopyFrom(ptrGlob);
    ptrValPar.IntComp = 5;
    nextRecord.IntComp = ptrValPar.IntComp;
    nextRecord.PtrComp = ptrValPar.PtrComp;
    Proc3(ref nextRecord.PtrComp);
    if (nextRecord.Discr == Ident1) {
      nextRecord.IntComp = 6;
      Proc6(ptrValPar.EnumComp, ref nextRecord.EnumComp);
      nextRecord.PtrComp = ptrGlob.PtrComp;
      Proc7(nextRecord.IntComp, 10, ref nextRecord.IntComp);
    } else
      ptrValPar.CopyFrom(ptrValPar.PtrComp);
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static void Proc2(ref long intParRef) {
    long intLoc = intParRef + 10;
    int enumLoc = Ident2;
    do
      if (ch1Glob == 'A') {
        intLoc--;
        intParRef = intLoc - intGlob;
        enumLoc = Ident1;
      }
    while (enumLoc != Ident1);
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static void Proc3(ref Record ptrRefPar) {
    if (ptrGlob != null) ptrRefPar = ptrGlob.PtrComp;
    Proc7(10, intGlob, ref ptrGlob.IntComp);
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static void Proc4() {
    bool boolLoc = ch1Glob == 'A';
    boolGlob = boolLoc | boolGlob;
    ch2Glob = 'B';
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static void Proc5() {
    ch1Glob = 'A';
    boolGlob = false;
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static void Proc6(int enumValPar, ref int enumRefPar) {
    enumRefPar = enumValPar;
    if (!Func3(enumValPar)) enumRefPar = Ident4;
    switch (enumValPar) {
    case Ident1: enumRefPar = Ident1; break;
    case Ident2: enumRefPar = intGlob > 100 ? Ident1 : Ident4; break;
    case Ident3: enumRefPar = Ident2; break;
    case Ident5: enumRefPar = Ident3; break;
    }
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static void Proc7(long int1ParVal, long int2ParVal, ref long intParRef) {
    long intLoc = int1ParVal + 2;
    intParRef = int2ParVal + intLoc;
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static void Proc8(long[] arr1ParRef, long[][] arr2ParRef, long int1ParVal, long int2ParVal) {
    long intLoc = int1ParVal + 5;
    arr1ParRef[intLoc] = int2ParVal;
    arr1ParRef[intLoc + 1] = arr1ParRef[intLoc];
    arr1ParRef[intLoc + 30] = intLoc;
    for (long intIndex = intLoc; intIndex <= intLoc + 1; intIndex++) arr2ParRef[intLoc][intIndex] = intLoc;
    arr2ParRef[intLoc][intLoc - 1]++;
    arr2ParRef[intLoc + 20][intLoc] = arr1ParRef[intLoc];
    intGlob = 5;
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static int Func1(char ch1ParVal, char ch2ParVal) {
    char ch1Loc = ch1ParVal, ch2Loc = ch1Loc;
    if (ch2Loc != ch2ParVal) return Ident1;
    ch1Glob = ch1Loc;
    return Ident2;
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static bool Func2(string str1ParRef, string str2ParRef) {
    int intLoc = 2;
    char chLoc = '\0';
    while (intLoc <= 2)
      if (Func1(str1ParRef[intLoc], str2ParRef[intLoc + 1]) == Ident1) {
        chLoc = 'A';
        intLoc++;
      }
    if (chLoc >= 'W' && chLoc < 'Z') intLoc = 7;
    if (chLoc == 'R') return true;
    if (string.CompareOrdinal(str1ParRef, str2ParRef) > 0) {
      intLoc += 7;
      intGlob = intLoc;
      return true;
    }
    return false;
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static bool Func3(int enumParVal) {
    int enumLoc = enumParVal;
    return enumLoc == Ident3;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    long int1Loc = 0, int2Loc = 0, int3Loc = 0;
    int enumLoc = Ident1;
    nextPtrGlob = new Record();
    ptrGlob = new Record {
      PtrComp = nextPtrGlob, Discr = Ident1, EnumComp = Ident3, IntComp = 40, StrComp = "DHRYSTONE PROGRAM, SOME STRING"
    };
    string str1Loc = "DHRYSTONE PROGRAM, 1'ST STRING", str2Loc = "";
    arr2Glob[8][7] = 10;
    for (long run = 1; run <= Runs; run++) {
      Proc5();
      Proc4();
      int1Loc = 2;
      int2Loc = 3;
      str2Loc = "DHRYSTONE PROGRAM, 2'ND STRING";
      enumLoc = Ident2;
      boolGlob = !Func2(str1Loc, str2Loc);
      while (int1Loc < int2Loc) {
        int3Loc = 5 * int1Loc - int2Loc;
        Proc7(int1Loc, int2Loc, ref int3Loc);
        int1Loc++;
      }
      Proc8(arr1Glob, arr2Glob, int1Loc, int3Loc);
      Proc1(ptrGlob);
      for (char ch = 'A'; ch <= ch2Glob; ch++) {
        if (enumLoc == Func1(ch, 'C')) {
          Proc6(Ident1, ref enumLoc);
          str2Loc = "DHRYSTONE PROGRAM, 3'RD STRING";
          int2Loc = run;
          intGlob = run;
        }
      }
      int2Loc = int2Loc * int1Loc;
      int1Loc = int2Loc / int3Loc;
      int2Loc = 7 * (int2Loc - int3Loc) - int1Loc;
      Proc2(ref int1Loc);
    }
    const string some = "DHRYSTONE PROGRAM, SOME STRING";
    bool ok = intGlob == 5 && boolGlob && ch1Glob == 'A' && ch2Glob == 'B' && arr1Glob[8] == 7 &&
              arr2Glob[8][7] == Runs + 10 && ptrGlob.PtrComp == nextPtrGlob && ptrGlob.Discr == Ident1 &&
              ptrGlob.EnumComp == Ident3 && ptrGlob.IntComp == 17 && ptrGlob.StrComp == some &&
              nextPtrGlob.PtrComp == nextPtrGlob && nextPtrGlob.Discr == Ident1 && nextPtrGlob.EnumComp == Ident2 &&
              nextPtrGlob.IntComp == 18 && nextPtrGlob.StrComp == some && int1Loc == 5 && int2Loc == 13 &&
              int3Loc == 7 && enumLoc == Ident2 && str1Loc == "DHRYSTONE PROGRAM, 1'ST STRING" &&
              str2Loc == "DHRYSTONE PROGRAM, 2'ND STRING";
    Program.Report("dhrystone", sw, ok);
  }
}

// Vectors are objects made and dropped, as the .prg's arrays of three doubles.
static class RayTracerBench {
  sealed class Vec {
    public readonly double X, Y, Z;
    public Vec(double x, double y, double z) {
      X = x;
      Y = y;
      Z = z;
    }
  }

  sealed class Sphere {
    public Vec C;
    public double R, Refl, Albedo;
  }

  static Vec Add(Vec a, Vec b) => new Vec(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
  static Vec Sub(Vec a, Vec b) => new Vec(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
  static Vec Scale(Vec a, double s) => new Vec(a.X * s, a.Y * s, a.Z * s);
  static double Dot(Vec a, Vec b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z;
  static Vec Norm(Vec a) => Scale(a, 1.0 / Math.Sqrt(Dot(a, a)));

  // The distance along d from o to sphere s, or 0 when the ray misses it.
  static double Intersect(Sphere s, Vec o, Vec d) {
    Vec oc = Sub(o, s.C);
    double b = Dot(oc, d), disc = b * b - (Dot(oc, oc) - s.R * s.R);
    if (disc > 0.0) {
      double q = Math.Sqrt(disc), t = -b - q;
      if (t < 0.0001) t = -b + q;
      if (t > 0.0001) return t;
    }
    return 0.0;
  }

  static double Trace(Sphere[] spheres, Vec light, Vec o, Vec d, int depth) {
    double best = 0.0;
    Sphere hit = null;
    foreach (var s in spheres) {
      double t = Intersect(s, o, d);
      if (t > 0.0 && (hit == null || t < best)) {
        best = t;
        hit = s;
      }
    }
    if (hit == null) return 0.2 + 0.5 * Math.Max(0.0, d.Y);
    Vec p = Add(o, Scale(d, best));
    Vec n = Scale(Sub(p, hit.C), 1.0 / hit.R);
    Vec toLight = Sub(light, p);
    double dist = Math.Sqrt(Dot(toLight, toLight));
    Vec l = Scale(toLight, 1.0 / dist);
    double diff = Dot(n, l);
    foreach (var s in spheres) {
      if (diff <= 0.0) break;
      double t = Intersect(s, p, l);
      if (t > 0.0 && t < dist) diff = 0.0;
    }
    double col = hit.Albedo * (0.1 + 0.9 * Math.Max(diff, 0.0));
    if (depth < 3 && hit.Refl > 0.0)
      col = col * (1.0 - hit.Refl) + hit.Refl * Trace(spheres, light, p, Sub(d, Scale(n, 2.0 * Dot(d, n))), depth + 1);
    return col;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int width = 320, height = 240;
    bool ok = true;
    // Each sphere: its center, its radius, how much it reflects and its albedo.
    var spheres = new[] {
      new Sphere { C = new Vec(0.0, -1000.0, 0.0), R = 999.0, Refl = 0.2, Albedo = 0.6 },
      new Sphere { C = new Vec(0.0, 0.0, 5.0), R = 1.0, Refl = 0.5, Albedo = 0.9 },
      new Sphere { C = new Vec(-2.0, 0.0, 6.0), R = 1.0, Refl = 0.3, Albedo = 0.7 },
      new Sphere { C = new Vec(2.0, 0.0, 6.0), R = 1.0, Refl = 0.3, Albedo = 0.8 },
      new Sphere { C = new Vec(0.0, 2.2, 7.0), R = 1.0, Refl = 0.6, Albedo = 0.5 },
    };
    var light = new Vec(5.0, 10.0, -5.0);
    var eye = new Vec(0.0, 0.5, -1.0);
    for (int rep = 1; rep <= 2; rep++) {
      long total = 0;
      for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
          Vec d = Norm(new Vec((x - width / 2.0 + 0.5) / height, (height / 2.0 - y - 0.5) / height, 1.0));
          total += (long)(Math.Min(Trace(spheres, light, eye, d, 0), 1.0) * 255.0);
        }
      ok = ok && total == 7100994;
    }
    Program.Report("ray_tracer", sw, ok);
  }
}

static class BlackScholesBench {
  // The cumulative normal distribution (the polynomial approximation of Abramowitz and Stegun, 26.2.17).
  static double Cnd(double d) {
    double k = 1.0 / (1.0 + 0.2316419 * Math.Abs(d));
    double w = 1.0 - 0.3989422804014327 * Math.Exp(-0.5 * d * d) *
      k * (0.319381530 + k * (-0.356563782 + k * (1.781477937 + k * (-1.821255978 + k * 1.330274429))));
    return d < 0.0 ? 1.0 - w : w;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 500000;
    long seed = 12345;
    double r = 0.02, v = 0.30, sumCall = 0.0, sumPut = 0.0;
    for (int i = 1; i <= n; i++) {
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      double s = 5.0 + 25.0 * seed / 2147483648.0;
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      double x = 1.0 + 99.0 * seed / 2147483648.0;
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      double tm = 0.25 + 9.75 * seed / 2147483648.0;
      double vst = v * Math.Sqrt(tm);
      double d1 = (Math.Log(s / x) + (r + v * v * 0.5) * tm) / vst;
      double d2 = d1 - vst;
      double disc = x * Math.Exp(-r * tm);
      sumCall += s * Cnd(d1) - disc * Cnd(d2);
      sumPut += disc * Cnd(-d2) - s * Cnd(-d1);
    }
    bool ok = Math.Abs(sumCall / 1494483.92385431 - 1) < 0.000000001 &&
              Math.Abs(sumPut / 15576648.71605097 - 1) < 0.000000001;
    Program.Report("black_scholes", sw, ok);
  }
}

static class JuliaSetBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int w = 1600, h = 1200, maxIter = 255;
    double cr = -0.8, ci = 0.156;
    var hist = new long[maxIter + 1];
    long total = 0, cs = 0;
    for (int y = 0; y < h; y++)
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
    for (int k = 0; k <= maxIter; k++) cs = (cs * 31 + hist[k]) % 2147483648;
    Program.Report("julia_set", sw, total == 69243973 && cs == 2105097618);
  }
}

static class MandelbrotBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 1600;
    long cs = 0, nBytes = 0;
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
        bits = (bits << 1) | (tr + ti <= 4.0 ? 1L : 0L);
        nBits++;
        if (nBits == 8 || x == n - 1) {
          bits <<= 8 - nBits;
          cs = (cs * 31 + bits) & 0x7FFFFFFF;
          nBytes++;
          bits = 0;
          nBits = 0;
        }
      }
    }
    Program.Report("mandelbrot", sw, cs == 366763831 && nBytes == n * ((n + 7) / 8));
  }
}

static class MonteCarloPiBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 50000000;
    long seed = 42, inside = 0;
    for (int i = 1; i <= n; i++) {
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      double x = seed / 2147483648.0;
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      double y = seed / 2147483648.0;
      if (x * x + y * y <= 1.0) inside++;
    }
    double est = 4.0 * inside / n;
    Program.Report("monte_carlo_pi", sw, inside == 39267523 && Math.Abs(est - 3.14159) < 0.01);
  }
}

// The functions are delegates, as the .prg's codeblocks.
static class NumericalIntegrationBench {
  // The integral of f from a to b by Simpson's rule over n intervals (n even).
  static double Simpson(Func<double, double> f, double a, double b, int n) {
    double h = (b - a) / n, s = f(a) + f(b);
    for (int i = 1; i <= n - 1; i++) s = s + ((i & 1) == 1 ? 4.0 : 2.0) * f(a + i * h);
    return s * h / 3.0;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 1000000;
    bool ok = true;
    for (int rep = 1; rep <= 3; rep++) {
      double r1 = Simpson(x => 4.0 / (1.0 + x * x), 0.0, 1.0, n);
      double r2 = Simpson(x => Math.Exp(-(x * x)), 0.0, 1.0, n);
      double r3 = Simpson(x => 1.0 / x, 1.0, 2.0, n);
      double r4 = Simpson(x => Math.Sqrt(x), 0.0, 1.0, n);
      ok = ok && Math.Abs(r1 - 3.141592653589793) < 0.000000000001 &&
           Math.Abs(r2 - 0.746824132812427) < 0.000000000001 && Math.Abs(r3 - 0.6931471805599453) < 0.000000000001 &&
           Math.Abs(r4 - 2.0 / 3.0) < 0.000000001;
    }
    Program.Report("numerical_integration", sw, ok);
  }
}

// Rows are arrays swapped as references, as the .prg's nested arrays; indices 1-based (element 0 unused).
static class GaussianEliminationBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 300;
    bool ok = true;
    for (int rep = 1; rep <= 6; rep++) {
      long seed = 42;
      var a = new double[n + 1][];
      for (int i = 1; i <= n; i++) {
        a[i] = new double[n + 2];
        double s = 0.0;
        for (int j = 1; j <= n; j++) {
          seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
          a[i][j] = seed / 2147483648.0 - 0.5;
          s = s + a[i][j];
        }
        a[i][n + 1] = s;
      }
      for (int k = 1; k <= n; k++) {
        int p = k;
        for (int i = k + 1; i <= n; i++)
          if (Math.Abs(a[i][k]) > Math.Abs(a[p][k])) p = i;
        double[] piv = a[p];
        a[p] = a[k];
        a[k] = piv;
        for (int i = k + 1; i <= n; i++) {
          double[] row = a[i];
          double f = row[k] / piv[k];
          for (int j = k; j <= n + 1; j++) row[j] = row[j] - f * piv[j];
        }
      }
      var x = new double[n + 1];
      double err = 0.0;
      for (int i = n; i >= 1; i--) {
        double[] row = a[i];
        double s = row[n + 1];
        for (int j = i + 1; j <= n; j++) s = s - row[j] * x[j];
        x[i] = s / row[i];
        err = Math.Max(err, Math.Abs(x[i] - 1.0));
      }
      ok = ok && err < 0.000000001;
    }
    Program.Report("gaussian_elimination", sw, ok);
  }
}

// The numbers are lists of 30-bit limbs (the least significant first), as the .prg's growing arrays.
static class PidigitsBench {
  const int Bits = 30;
  const long LimbMask = 0x3FFFFFFF;
  const double Radix = 1073741824.0;

  // Drops the leading zero limbs (keeps one).
  static void Normalize(List<long> a) {
    int n = a.Count;
    while (n > 1 && a[n - 1] == 0) n--;
    if (n < a.Count) a.RemoveRange(n, a.Count - n);
  }

  // a *= m
  static void MulSmall(List<long> a, long m) {
    long carry = 0;
    for (int i = 0; i < a.Count; i++) {
      long x = a[i] * m + carry;
      a[i] = x & LimbMask;
      carry = x >> Bits;
    }
    while (carry > 0) {
      a.Add(carry & LimbMask);
      carry >>= Bits;
    }
    Normalize(a);
  }

  // a += b * m
  static void AddMul(List<long> a, List<long> b, long m) {
    long carry = 0;
    int nb = b.Count, i;
    while (a.Count < nb) a.Add(0);
    for (i = 0; i < nb; i++) {
      long x = a[i] + b[i] * m + carry;
      a[i] = x & LimbMask;
      carry = x >> Bits;
    }
    for (; carry > 0 && i < a.Count; i++) {
      long x = a[i] + carry;
      a[i] = x & LimbMask;
      carry = x >> Bits;
    }
    while (carry > 0) {
      a.Add(carry & LimbMask);
      carry >>= Bits;
    }
  }

  // a -= b * m, when b * m <= a
  static void SubMul(List<long> a, List<long> b, long m) {
    long borrow = 0;
    int i;
    for (i = 0; i < b.Count; i++) {
      long x = a[i] - b[i] * m - borrow;
      a[i] = x & LimbMask;
      borrow = -(x >> Bits);
    }
    for (; borrow > 0; i++) {
      long x = a[i] - borrow;
      a[i] = x & LimbMask;
      borrow = -(x >> Bits);
    }
    Normalize(a);
  }

  // a += b * m (m >= 0), neg the sign of a, termNeg that of the term.
  static void AddSigned(List<long> a, ref bool neg, List<long> b, long m, bool termNeg) {
    if (neg == termNeg)
      AddMul(a, b, m);
    else {
      var tmp = new List<long>(b);
      MulSmall(tmp, m);
      if (Cmp(a, tmp) >= 0)
        SubMul(a, tmp, 1);
      else {
        SubMul(tmp, a, 1);
        a.Clear();
        a.AddRange(tmp);
        neg = termNeg;
      }
    }
    if (a.Count == 1 && a[0] == 0) neg = false;
  }

  static int Cmp(List<long> a, List<long> b) {
    if (a.Count != b.Count) return a.Count < b.Count ? -1 : 1;
    for (int i = a.Count - 1; i >= 0; i--)
      if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
  }

  // The limb at the 1-based i as a double, 0 past either end.
  static double Top(List<long> a, int i) => i >= 1 && i <= a.Count ? a[i - 1] * 1.0 : 0.0;

  // Int( x / d ) for a quotient below 2^30: estimated from the three leading limbs, then corrected.
  static long Quotient(List<long> x, List<long> d) {
    int h = Math.Max(x.Count, d.Count);
    double fx = (Top(x, h) * Radix + Top(x, h - 1)) * Radix + Top(x, h - 2);
    double fd = (Top(d, h) * Radix + Top(d, h - 1)) * Radix + Top(d, h - 2);
    long q = (long)(fx / fd);
    var tmp = new List<long>(d);
    MulSmall(tmp, q);
    while (Cmp(tmp, x) > 0) {
      SubMul(tmp, d, 1);
      q--;
    }
    AddMul(tmp, d, 1);
    while (Cmp(tmp, x) <= 0) {
      AddMul(tmp, d, 1);
      q++;
    }
    return q;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 1500;
    var digits = new StringBuilder();
    bool neg = false;
    long k = 0, sum = 0;
    var numer = new List<long> { 1 };
    var accum = new List<long> { 0 };
    var denom = new List<long> { 1 };
    while (digits.Length < n) {
      k++;
      AddSigned(accum, ref neg, numer, 2, false);
      MulSmall(accum, 2 * k + 1);
      MulSmall(denom, 2 * k + 1);
      MulSmall(numer, k);
      if (neg || Cmp(numer, accum) > 0) continue;
      var x = new List<long>(accum);
      bool xNeg = neg;
      AddSigned(x, ref xNeg, numer, 3, false);
      long d = Quotient(x, denom);
      AddSigned(x, ref xNeg, numer, 1, false);
      if (d != Quotient(x, denom)) continue;
      digits.Append((char)(48 + d));
      AddSigned(accum, ref neg, denom, d, true);
      MulSmall(accum, 10);
      MulSmall(numer, 10);
    }
    string s = digits.ToString();
    for (int i = 0; i < n; i++) sum += s[i] - 48;
    bool ok = s.StartsWith("31415926535897932384626433832795028841971693993751", StringComparison.Ordinal) &&
              s.EndsWith("55570674983850549458858692699569092721079750930295", StringComparison.Ordinal) && sum == 6775;
    Program.Report("pidigits", sw, ok);
  }
}

static class RandomMassivoBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 30000000;
    var hist = new long[256];
    long lcg = 12345, xs64 = 88172645463325252, z = 362436069, w = 521288629, xs32 = 2463534242;
    long s1 = 0, s2 = 0, s3 = 0, s4 = 0, check = 0, r;
    double fsum = 0.0;
    for (int i = 1; i <= n; i++) {
      lcg = (lcg * 1103515245 + 12345) & 0x7FFFFFFF;
      s1 += lcg;
      hist[lcg >> 23]++;
      fsum += lcg / 2147483648.0;

      xs64 ^= xs64 << 13;
      xs64 ^= (xs64 >> 7) & 0x01FFFFFFFFFFFFFF;
      xs64 ^= xs64 << 17;
      r = (xs64 >> 32) & 0xFFFFFFFF;
      s2 += r;
      hist[r >> 24]++;

      z = 36969 * (z & 0xFFFF) + (z >> 16);
      w = 18000 * (w & 0xFFFF) + (w >> 16);
      r = ((z << 16) + w) & 0xFFFFFFFF;
      s3 += r;
      hist[r >> 24]++;

      xs32 ^= (xs32 << 13) & 0xFFFFFFFF;
      xs32 ^= xs32 >> 17;
      xs32 ^= (xs32 << 5) & 0xFFFFFFFF;
      s4 += xs32;
      hist[xs32 >> 24]++;
    }
    for (int i = 1; i <= 256; i++) check += hist[i - 1] * i;
    bool ok = s1 == 32217797789519680 && s2 == 64428429046975605 && s3 == 64423232821249815 &&
              s4 == 64428119329011698 && check == 15421035017 && Math.Abs(fsum - 15002581.187856009) < 0.000001;
    Program.Report("random_massivo", sw, ok);
  }
}
