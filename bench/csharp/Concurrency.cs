// bench/tasks, channels, async, async_pi and async_generator .prg in C#: the same work and checks, with .NET's Task,
// Channel, async/await and asynchronous iterators (IAsyncEnumerable, await foreach).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading.Channels;
using System.Threading.Tasks;

static class TasksBench {
  public static Task Run() {
    var sw = Stopwatch.StartNew();
    var a = new Task<long>[200000];
    for (int k = 0; k < a.Length; k++) {
      long i = k + 1;
      a[k] = Task.Run(() => i % 7);
    }
    long s = Task.WhenAll(a).Result.Sum();
    Task<long> c = Task.FromResult(0L);
    for (int k = 1; k <= 50000; k++) c = c.ContinueWith(p => p.Result + 1);
    s += c.Result;
    for (int k = 1; k <= 20000; k++) s += Task.Run(() => Task.Run(() => 1L).Result).Result;
    Program.Report("tasks", sw, s == 599997 + 50000 + 20000);
    return Task.CompletedTask;
  }
}

static class ChannelsBench {
  public static async Task Run() {
    var sw = Stopwatch.StartNew();
    var ch = Channel.CreateBounded<long>(64);
    // The tasks block their threads while they wait, as the .prg's do (SendAsync( i ):Wait(), FOR EACH x IN ch).
    var consumers = Enumerable.Range(0, 2).Select(_ => Task.Run(() => {
      long s = 0;
      while (ch.Reader.WaitToReadAsync().AsTask().Result)
        while (ch.Reader.TryRead(out long x)) s += x;
      return s;
    })).ToArray();
    var producers = Enumerable.Range(0, 4).Select(_ => Task.Run(() => {
      for (long i = 1; i <= 250000; i++) ch.Writer.WriteAsync(i).AsTask().Wait();
    })).ToArray();
    await Task.WhenAll(producers);
    ch.Writer.Complete();
    long total = (await Task.WhenAll(consumers)).Sum();
    var u = Channel.CreateUnbounded<long>();
    for (long k = 1; k <= 1000000; k++) {
      u.Writer.TryWrite(k);
      u.Reader.TryRead(out long v);
      total += v % 2;
    }
    Program.Report("channels", sw, total == 4 * 31250125000L + 500000);
  }
}

static class AsyncBench {
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

  public static async Task Run() {
    var sw = Stopwatch.StartNew();
    long s = 0;
    for (long k = 1; k <= 1000000; k++) s += await Sync(k);
    s += await PingPong(100000);
    var h = new Task<long>[10000];
    for (int k = 1; k <= h.Length; k++) h[k - 1] = Handler(k);
    s += (await Task.WhenAll(h)).Sum();
    Program.Report("async", sw, s == 500000500000 + 5000050000 + 50005000);
  }
}

static class AsyncPi {
  const long Terms = 50000000;

  static async Task<double> Partial(int i) => await Task.Run(() => {
    double s = 0;
    for (long n = i * Terms + 1; n <= (i + 1) * Terms; n++) {
      double inv = 1.0 / n;
      s += inv * inv;
    }
    return s;
  });

  public static async Task Run() {
    var sw = Stopwatch.StartNew();
    double[] partial = await Task.WhenAll(Enumerable.Range(0, 32).Select(Partial));
    double pi = Math.Sqrt(partial.Sum() * 6);
    Program.Report("async_pi", sw, Math.Abs(Math.PI - pi) < 1e-9);
  }
}

static class AsyncGeneratorBench {
  static async IAsyncEnumerable<long> Source(long n) {
    for (long k = 1; k <= n; k++) {
      if (k % 1000 == 0) await Task.Yield();
      yield return k;
    }
  }

  static async IAsyncEnumerable<long> Evens(IAsyncEnumerable<long> g) {
    await foreach (var x in g)
      if (x % 2 == 0) yield return x;
  }

  static async IAsyncEnumerable<long> Doubled(IAsyncEnumerable<long> g) {
    await foreach (var x in g) yield return x * 2;
  }

  public static async Task Run() {
    var sw = Stopwatch.StartNew();
    long s = 0;
    await foreach (var x in Doubled(Evens(Source(1000000)))) s += x;
    Program.Report("async_generator", sw, s == 500001000000);
  }
}
