// The same work as bench/async_pi.prg: the Basel sum over 32 async methods, each awaiting a CPU-bound Task.Run of
// 50,000,000 terms; pi is Sqrt( 6 * sum ). Prints `async_pi <milliseconds> ok|WRONG` as the xBase benchmarks do.
using System;
using System.Diagnostics;
using System.Linq;
using System.Threading.Tasks;

static class Program {
  const int TaskCount = 32;
  const long TermsPerTask = 50_000_000;

  static async Task<double> Partial(int i) {
    return await Task.Run(() => {
      double s = 0;
      for (long n = i * TermsPerTask + 1; n <= (i + 1) * TermsPerTask; n++) {
        double inv = 1.0 / n;
        s += inv * inv;
      }
      return s;
    });
  }

  static async Task Main() {
    var sw = Stopwatch.StartNew();
    double[] partial = await Task.WhenAll(Enumerable.Range(0, TaskCount).Select(Partial));
    double pi = Math.Sqrt(partial.Sum() * 6);
    Console.WriteLine($"async_pi {sw.ElapsedMilliseconds} {(Math.Abs(Math.PI - pi) < 1e-9 ? "ok" : "WRONG")}");
  }
}
