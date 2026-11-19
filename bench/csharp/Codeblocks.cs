// bench/asort_codeblock, aeval_heval, codeblock_callback, codeblock_eval, map_filter_reduce, closure_capture and
// higher_order_chain .prg in C#: the same work and checks, codeblocks as delegates (lambdas that capture what the
// blocks capture), the runtime's Enumerable methods as LINQ.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;

static class AsortCodeblockBench {
  sealed class Employee {
    public string Name;
    public long Department, Salary, Id;
  }

  static readonly MinStd random = new MinStd(4242);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    var a = new Employee[150000];
    for (int i = 0; i < a.Length; i++) {
      var c = new StringBuilder();
      for (int k = 1; k <= 8; k++) c.Append((char)(65 + Rand(26)));
      var e = new Employee { Name = c.ToString() };
      e.Department = Rand(40) + 1;
      e.Salary = Rand(90000) + 1000;
      e.Id = i + 1;
      a[i] = e;
    }
    var sw = Stopwatch.StartNew();
    // Stable as ASort is: OrderBy keeps equal records in their order.
    var order = Comparer<Employee>.Create((p, q) => p.Department != q.Department ? p.Department.CompareTo(q.Department)
                                                    : p.Salary != q.Salary ? q.Salary.CompareTo(p.Salary)
                                                    : string.CompareOrdinal(p.Name, q.Name));
    a = a.OrderBy(x => x, order).ToArray();
    long chk = 0;
    foreach (var e in a) chk = (chk * 31 + e.Id) % 1000000007;
    bool ok = chk == 773935306 && a[0].Id == 84060 && a[0].Name == "SOFHDANH" && a[75000].Id == 10853 &&
              a[149999].Id == 11664;
    Program.Report("asort_codeblock", sw, ok);
  }
}

static class AevalHevalBench {
  // AEval( a, b ): b( element, position ).
  static void AEval(long[] a, Action<long, int> b) {
    for (int i = 0; i < a.Length; i++) b(a[i], i + 1);
  }

  // HEval( h, b ): b( key, value ).
  static void HEval(Dictionary<long, long> h, Action<long, long> b) {
    foreach (var (k, v) in h) b(k, v);
  }

  public static void Run() {
    var a = new long[1000000];
    var h = new Dictionary<long, long>();
    for (int i = 1; i <= a.Length; i++) a[i - 1] = i % 777;
    for (long i = 1; i <= 200000; i++) h[i * 3] = i % 555;
    var sw = Stopwatch.StartNew();
    long n = 0;
    for (int r = 1; r <= 20; r++) {
      AEval(a, (x, pos) => n += x + (pos % 1000 == 0 ? r : 0));
      HEval(h, (k, v) => n += v + (k % 3000 == 0 ? 1 : 0));
    }
    Program.Report("aeval_heval", sw, n == 8867500260);
  }
}

static class CodeblockCallbackBench {
  static readonly MinStd random = new MinStd(1);
  static long Rand(long n) => random.Next(n);

  static double Simpson(Func<double, double> f, double a, double b, int n) {
    double h = (b - a) / n, sum = f(a) + f(b);
    for (int i = 1; i <= n - 1; i++) sum += f(a + i * h) * (i % 2 == 1 ? 4 : 2);
    return sum * h / 3;
  }

  static void InsertionSort(long[] a, Func<long, long, bool> less) {
    for (int i = 1; i < a.Length; i++) {
      long x = a[i];
      int j = i - 1;
      while (j >= 0 && less(x, a[j])) {
        a[j + 1] = a[j];
        j--;
      }
      a[j + 1] = x;
    }
  }

  public static void Run() {
    var data = new long[3000];
    for (int i = 0; i < data.Length; i++) data[i] = Rand(100000);
    var sw = Stopwatch.StartNew();
    double integral = 0;
    long check = 0;
    integral += Simpson(x => x * x, 0, 3, 1000000);
    integral += Simpson(x => 1 / (1 + x * x), 0, 1, 1000000);
    integral += Simpson(x => x * x * x - 2 * x, -1, 2, 1000000);
    for (int r = 1; r <= 10; r++) {
      var a = (long[])data.Clone();
      InsertionSort(a, r % 2 == 0 ? (x, y) => x < y : (x, y) => x > y);
      check += a[0] + a[1499] * 2 + a[2999] * 3;
    }
    Program.Report("codeblock_callback", sw, Math.Abs(integral - 10.535398163397) < 0.000000001 && check == 2991620);
  }
}

static class CodeblockEvalBench {
  // The three blocks in an array, as the .prg keeps them.
  static Delegate[] Blocks(long nBase) =>
    new Delegate[] { new Func<long>(() => 7), new Func<long, long, long>((x, y) => x * 2 + y),
                     new Func<long, long>(x => x + nBase) };

  public static void Run() {
    var sw = Stopwatch.StartNew();
    Delegate[] blocks = Blocks(3);
    long n = 0;
    for (long i = 1; i <= 10000000; i++)
      n += ((Func<long>)blocks[0])() + ((Func<long, long, long>)blocks[1])(i % 100, 1) +
           ((Func<long, long>)blocks[2])(i % 10);
    Program.Report("codeblock_eval", sw, n == 1145000000);
  }
}

// Map, Filter and Reduce written out, as the .prg writes them.
static class MapFilterReduceBench {
  static long[] Map(long[] a, Func<long, long> b) {
    var output = new long[a.Length];
    for (int i = 0; i < a.Length; i++) output[i] = b(a[i]);
    return output;
  }

  static List<long> Filter(long[] a, Func<long, bool> b) {
    var output = new List<long>();
    foreach (long x in a)
      if (b(x)) output.Add(x);
    return output;
  }

  static long Reduce(List<long> a, Func<long, long, long> b, long acc) {
    foreach (long x in a) acc = b(acc, x);
    return acc;
  }

  public static void Run() {
    var a = new long[1000000];
    for (int i = 1; i <= a.Length; i++) a[i - 1] = i % 1000;
    var sw = Stopwatch.StartNew();
    long n = 0;
    for (long r = 1; r <= 8; r++) n += Reduce(Filter(Map(a, x => x * r + 1), x => x % 3 == 0), (acc, x) => acc + x, 0);
    Program.Report("map_filter_reduce", sw, n == 4492503000);
  }
}

static class ClosureCaptureBench {
  static Func<long, long> Counter(long step) {
    long count = 0;
    return x => count += step + x;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var a = new Func<long, long>[500000];
    long n = 0, shared = 0;
    for (int i = 1; i <= a.Length; i++) a[i - 1] = Counter(i % 10);
    for (long j = 1; j <= 4; j++)
      foreach (var b in a) n += b(j);
    Func<long, long> inc = x => shared += x;
    Func<long> read = () => shared % 1000;
    for (long i = 1; i <= 2000000; i++) {
      inc(i % 7);
      n += read();
    }
    Program.Report("closure_capture", sw, n == 1031500997 && shared == 5999997);
  }
}

static class HigherOrderChainBench {
  public static void Run() {
    var a = new long[500000];
    for (int i = 1; i <= a.Length; i++) a[i - 1] = i * 37L % 1000;
    var sw = Stopwatch.StartNew();
    long n = 0;
    for (long r = 1; r <= 5; r++) {
      n += a.Where(x => x % 3 != 0).Select(x => x * r).Skip(10).TakeWhile(x => x < 4900).Sum();
      n += a.Select((x, pos) => x + (pos + 1) % 7).Where(x => x % 2 == 0).Take((int)(50000 + r)).Count();
      n += a.DistinctBy(x => x % (100 * r)).Count();
    }
    Program.Report("higher_order_chain", sw, n == 1663633050);
  }
}
