// The same work as bench/async.prg: a million async calls that complete at once, awaited; a hundred thousand round
// trips between two async methods over channels of one; ten thousand handlers suspended at once on delays. Prints
// `async <milliseconds> ok|WRONG` as the xBase benchmarks do.
using System;
using System.Diagnostics;
using System.Threading.Channels;
using System.Threading.Tasks;

static class Program {
  static async Task<long> Sync(long k) => k;

  static async Task<long> PingPong(int n) {
    var a = Channel.CreateBounded<long>(1);
    var b = Channel.CreateBounded<long>(1);
    var t = Echo(a, b, n);
    long s = 0;
    for (int k = 1; k <= n; k++) {
      await a.Writer.WriteAsync(k);
      s += await b.Reader.ReadAsync();
    }
    await t;
    return s;
  }

  static async Task Echo(Channel<long> a, Channel<long> b, int n) {
    for (int k = 1; k <= n; k++) await b.Writer.WriteAsync(await a.Reader.ReadAsync());
  }

  static async Task<long> Handler(int k) {
    await Task.Delay(k % 10);
    await Task.Run(() => { });
    return k;
  }

  static async Task<long> Handlers(int n) {
    var a = new Task<long>[n];
    for (int k = 1; k <= n; k++) a[k - 1] = Handler(k);
    long s = 0;
    foreach (var x in await Task.WhenAll(a)) s += x;
    return s;
  }

  static async Task Main() {
    var sw = Stopwatch.StartNew();
    long s = 0;
    for (long k = 1; k <= 1000000; k++) s += await Sync(k);
    s += await PingPong(100000);
    s += await Handlers(10000);
    bool ok = s == 500000500000 + 5000050000 + 50005000;
    Console.WriteLine($"async {sw.ElapsedMilliseconds} {(ok ? "ok" : "WRONG")}");
  }
}
