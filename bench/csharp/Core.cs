// bench/fib, loops, sieve, matmul, qsort, asort, strings, crc32, float, mixed .prg in C#: the same work and checks.
using System;
using System.Diagnostics;

static class Fib {
  static long F(long n) => n < 2 ? n : F(n - 1) + F(n - 2);
  public static void Run() {
    var sw = Stopwatch.StartNew();
    Program.Report("fib", sw, F(37) == 24157817);
  }
}

static class Loops {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    long n = 0, limit = 500000000;
    for (long i = 1; i <= limit; i++) n = n + i * 2 - 1;
    Program.Report("loops", sw, n == 250000000000000000);
  }
}

static class Sieve {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 2000000;
    var a = new bool[n + 1];
    bool ok = true;
    for (int rep = 1; rep <= 10; rep++) {
      int count = 0;
      for (int i = 2; i <= n; i++) a[i] = true;
      a[1] = false;
      for (int i = 2; i * i <= n; i++)
        if (a[i])
          for (int j = i * i; j <= n; j += i) a[j] = false;
      for (int i = 1; i <= n; i++)
        if (a[i]) count++;
      ok = ok && count == 148933;
    }
    Program.Report("sieve", sw, ok);
  }
}

static class MatMul {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 100;
    var a = new long[n][];
    var b = new long[n][];
    var c = new long[n][];
    for (int i = 0; i < n; i++) {
      a[i] = new long[n];
      b[i] = new long[n];
      c[i] = new long[n];
      for (int j = 0; j < n; j++) {
        a[i][j] = (i + j) % 7;
        b[i][j] = (i * j) % 5;
      }
    }
    long total = 0;
    for (int rep = 1; rep <= 20; rep++)
      for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
          long s = 0;
          for (int k = 0; k < n; k++) s += a[i][k] * b[k][j];
          c[i][j] = s;
          total += s;
        }
    Program.Report("matmul", sw, total == 20L * 4798200);
  }
}

static class QSortBench {
  static void Qs(long[] a, long lo, long hi) {
    while (lo < hi) {
      long pivot = a[(lo + hi) / 2], i = lo, j = hi;
      while (i <= j) {
        while (a[i] < pivot) i++;
        while (a[j] > pivot) j--;
        if (i <= j) {
          (a[i], a[j]) = (a[j], a[i]);
          i++;
          j--;
        }
      }
      if (j - lo < hi - i) {
        Qs(a, lo, j);
        lo = i;
      } else {
        Qs(a, i, hi);
        hi = j;
      }
    }
  }

  static bool SortRandom() {
    int n = 200000;
    var a = new long[n + 1]; // 1-based, as the xBase array
    long x = 12345, sum = 0;
    bool sorted = true;
    for (int i = 1; i <= n; i++) {
      x = (x * 16807) % 2147483647;
      a[i] = x % 1000000;
    }
    Qs(a, 1, n);
    for (int i = 2; i <= n; i++)
      if (a[i - 1] > a[i]) sorted = false;
    for (int i = 1; i <= n; i += 1000) sum += a[i];
    return sorted && a[1] == 6 && a[100000] == 500294 && a[n] == 999998 && sum == 99556483;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    bool ok = true;
    for (int rep = 1; rep <= 5; rep++) ok = ok && SortRandom();
    Program.Report("qsort", sw, ok);
  }
}

static class ASortBench {
  static bool SortBoth() {
    var a = new long[300000];
    var b = new long[100000];
    long x = 777, sum = 0;
    for (int i = 0; i < a.Length; i++) {
      x = (x * 16807) % 2147483647;
      a[i] = x % 1000000;
    }
    x = 4242;
    for (int i = 0; i < b.Length; i++) {
      x = (x * 16807) % 2147483647;
      b[i] = x % 1000000;
    }
    Array.Sort(a);
    Array.Sort(b, (p, q) => q.CompareTo(p));
    for (int i = 0; i < a.Length; i += 1000) sum += a[i];
    return a[0] == 8 && a[149999] == 500673 && a[299999] == 999998 && sum == 149589926 && b[0] == 999994 &&
           b[49999] == 501282 && b[99999] == 8;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    bool ok = true;
    for (int rep = 1; rep <= 5; rep++) ok = ok && SortBoth();
    Program.Report("asort", sw, ok);
  }
}

static class Strings {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    string src = "abcdefghij";
    long total = 0;
    for (int i = 1; i <= 20000000; i++) {
      string w = src.Substring(0, i % 10 + 1).ToUpperInvariant();
      total += w.Length + w.IndexOf('D') + 1;
    }
    Program.Report("strings", sw, total == 166000000);
  }
}

static class Crc32 {
  static uint Of20K() {
    var table = new uint[256];
    for (uint i = 0; i < 256; i++) {
      uint c = i;
      for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
      table[i] = c;
    }
    var data = new System.Text.StringBuilder();
    for (int i = 0; i < 20000; i += 100) {
      var chunk = new System.Text.StringBuilder();
      for (int k = i; k < i + 100; k++) chunk.Append((char)((k * 31 + 7) % 251));
      data.Append(chunk);
    }
    uint crc = 0xFFFFFFFFu;
    for (int i = 0; i < data.Length; i++) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    bool ok = true;
    for (int rep = 1; rep <= 100; rep++) ok = ok && Of20K() == 2954121016u;
    Program.Report("crc32", sw, ok);
  }
}

static class FloatBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int size = 250, maxIter = 100;
    long total = 0;
    for (int rep = 1; rep <= 6; rep++)
      for (int y = 0; y < size; y++) {
        double ci = y * 2.0 / size - 1.0;
        for (int x = 0; x < size; x++) {
          double cr = x * 3.0 / size - 2.0, zr = 0.0, zi = 0.0;
          int k = 0;
          while (k < maxIter) {
            double zr2 = zr * zr, zi2 = zi * zi;
            if (zr2 + zi2 > 4.0) break;
            zi = 2.0 * zr * zi + ci;
            zr = zr2 - zi2 + cr;
            k++;
          }
          total += k;
        }
      }
    Program.Report("float", sw, total == 11391384);
  }
}

static class Mixed {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    double s = 0;
    long n = 0;
    for (long i = 1; i <= 20000000; i++) {
      double d = i + 3.14;
      s += (long)(d * 4) % 7;
      if (d > i + 3) n++;
      s += i / 4.0;
    }
    Program.Report("mixed", sw, s == 50000062499998.0 && n == 20000000);
  }
}
