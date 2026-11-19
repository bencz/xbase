// bench/producer_consumer, work_stealing, parallel_map, shared_counter, pipeline_tasks and chameneos_redux .prg in
// C#: the same work and checks, with .NET's threads, Monitor, Interlocked, Task and Channel where the .prg uses the
// runtime's threads, Mutex and Condition, AtomicInt, tasks and channels.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;

// The queue guarded by a Monitor: its one condition stands for the .prg's two (not full, not empty), so a change wakes
// every waiter (PulseAll), each testing its own condition again.
static class ProducerConsumerBench {
  const int Capacity = 64;

  static void Put(List<long?> q, long? x) {
    lock (q) {
      while (q.Count >= Capacity) Monitor.Wait(q);
      q.Add(x);
      Monitor.PulseAll(q);
    }
  }

  // Producer p puts p * 1000000 + i for i from 1 to n.
  static void Produce(List<long?> q, long p, long n) {
    for (long i = 1; i <= n; i++) Put(q, p * 1000000 + i);
  }

  // Takes numbers until the end mark (null); returns { their sum, their count }.
  static long[] Consume(List<long?> q) {
    long sum = 0, count = 0;
    while (true) {
      long? x;
      lock (q) {
        while (q.Count == 0) Monitor.Wait(q);
        x = q[0];
        q.RemoveAt(0);
        Monitor.PulseAll(q);
      }
      if (x == null) break;
      sum += x.Value;
      count++;
    }
    return new[] { sum, count };
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var q = new List<long?>();
    long items = 50000, sum = 0, count = 0;
    var results = new long[4][];
    var consumers = new Thread[4];
    var producers = new Thread[4];
    for (int k = 0; k < 4; k++) {
      int c = k;
      consumers[k] = new Thread(() => results[c] = Consume(q));
      consumers[k].Start();
    }
    for (int k = 0; k < 4; k++) {
      long p = k + 1;
      producers[k] = new Thread(() => Produce(q, p, items));
      producers[k].Start();
    }
    foreach (var t in producers) t.Join();
    for (int k = 0; k < 4; k++) Put(q, null);
    for (int k = 0; k < 4; k++) {
      consumers[k].Join();
      sum += results[k][0];
      count += results[k][1];
    }
    Program.Report("producer_consumer", sw, count == 4 * items && sum == 10000000 * items + 2 * items * (items + 1));
  }
}

// A task waiting for a child's Result runs it inline when no worker has taken it yet, as the .prg's do.
static class WorkStealingBench {
  static long Hash(long s) {
    for (int i = 1; i <= 40; i++) s = s * 16807 % 2147483647;
    return s;
  }

  static void Visit(long s, int d, ref long count, ref long sum) {
    long h = Hash(s);
    count++;
    sum += h % 1000;
    if (d < 5 || h % 1000 < 198)
      for (int k = 1; k <= (d < 5 ? 8 : 5); k++) Visit((s * 16807 + k * 48271) % 2147483647, d + 1, ref count, ref sum);
  }

  // { nodes, sum } of the subtree of seed s at depth d: its children as tasks down to depth 4, then walked in place.
  static (long Count, long Sum) Branch(long s, int d) {
    long count = 0, sum = 0;
    if (d >= 4) {
      Visit(s, d, ref count, ref sum);
      return (count, sum);
    }
    long h = Hash(s);
    var tasks = new Task<(long Count, long Sum)>[8];
    for (int k = 1; k <= 8; k++) {
      long c = (s * 16807 + k * 48271) % 2147483647;
      tasks[k - 1] = Task.Run(() => Branch(c, d + 1));
    }
    for (int k = 8; k >= 1; k--) {
      var a = tasks[k - 1].Result;
      count += a.Count;
      sum += a.Sum;
    }
    return (count + 1, sum + h % 1000);
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var a = Branch(19937, 0);
    Program.Report("work_stealing", sw, a.Count == 2859034 && a.Sum == 1428166988);
  }
}

static class ParallelMapBench {
  static long[] MapSlice(long[] input, int from, int count) {
    var a = new long[count];
    for (int i = 0; i < count; i++) a[i] = Steps(input[from + i]);
    return a;
  }

  static long Steps(long x) {
    long s = 0;
    while (x != 1) {
      x = x % 2 == 0 ? x / 2 : 3 * x + 1;
      s++;
    }
    return s;
  }

  public static async Task Run() {
    int n = 1000000;
    var input = new long[n];
    var output = new long[n];
    long x = 777, sum = 0, max = 0, at = 0, chk = 0;
    for (int i = 0; i < n; i++) {
      x = x * 16807 % 2147483647;
      input[i] = x % 1000000 + 1;
    }
    var sw = Stopwatch.StartNew();
    int chunk = n / 64, pos = 0;
    var tasks = new List<Task<long[]>>();
    for (int i = 0; i < n; i += chunk) {
      int from = i;
      tasks.Add(Task.Run(() => MapSlice(input, from, chunk)));
    }
    foreach (long[] a in await Task.WhenAll(tasks)) {
      Array.Copy(a, 0, output, pos, a.Length);
      pos += a.Length;
    }
    for (int i = 0; i < n; i++) {
      x = output[i];
      sum += x;
      if (x > max) {
        max = x;
        at = i + 1;
      }
      chk = (chk * 31 + x) % 1000000007;
    }
    Program.Report("parallel_map", sw, pos == n && sum == 131359344 && max == 524 && at == 237731 && chk == 68222219);
  }
}

static class SharedCounterBench {
  static long atomic, cas;
  static readonly long[] box = new long[1];

  static void Work() {
    for (int i = 1; i <= 250000; i++) Interlocked.Add(ref atomic, 1);
    for (int i = 1; i <= 50000; i++)
      while (true) {
        long n = Interlocked.Read(ref cas);
        if (Interlocked.CompareExchange(ref cas, n + 1, n) == n) break;
      }
    for (int i = 1; i <= 50000; i++)
      lock (box) box[0]++;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var threads = new Thread[8];
    for (int k = 0; k < threads.Length; k++) {
      threads[k] = new Thread(Work);
      threads[k].Start();
    }
    foreach (var t in threads) t.Join();
    Program.Report("shared_counter", sw, atomic == 8 * 250000 && cas == 8 * 50000 && box[0] == 8 * 50000);
  }
}

// The records between the stages are value tuples where the .prg sends arrays of two or three.
static class PipelineTasksBench {
  static async Task Source(Channel<string> output, int n) {
    long x = 4321;
    for (int i = 1; i <= n; i++) {
      x = x * 16807 % 2147483647;
      await output.Writer.WriteAsync(i + ";" + x % 100000);
    }
    output.Writer.Complete();
  }

  static async Task Parser(Channel<string> input, Channel<(long Id, long Value)> output) {
    await foreach (string c in input.Reader.ReadAllAsync()) {
      int n = c.IndexOf(';');
      await output.Writer.WriteAsync((long.Parse(c.AsSpan(0, n)), long.Parse(c.AsSpan(n + 1))));
    }
    output.Writer.Complete();
  }

  static async Task Square(Channel<(long Id, long Value)> input, Channel<(long Id, long Value)> output) {
    await foreach (var a in input.Reader.ReadAllAsync()) {
      long y = a.Value * a.Value % 1000003;
      if (y % 2 == 0) await output.Writer.WriteAsync((a.Id, y));
    }
    output.Writer.Complete();
  }

  // { the sum of the values, their count, the sum of their ids }
  static async Task<(long Sum, long Count, long Ids)> Sink(Channel<(long Id, long Value)> input) {
    long sum = 0, count = 0, ids = 0;
    await foreach (var a in input.Reader.ReadAllAsync()) {
      sum += a.Value;
      count++;
      ids += a.Id;
    }
    return (sum, count, ids);
  }

  public static async Task Run() {
    var sw = Stopwatch.StartNew();
    var c1 = Channel.CreateBounded<string>(256);
    var c2 = Channel.CreateBounded<(long Id, long Value)>(256);
    var c3 = Channel.CreateBounded<(long Id, long Value)>(256);
    Task source = Source(c1, 200000), parser = Parser(c1, c2), square = Square(c2, c3);
    Task<(long Sum, long Count, long Ids)> sink = Sink(c3);
    var a = await sink;
    await Task.WhenAll(source, parser, square, sink);
    Program.Report("pipeline_tasks", sw, a.Sum == 49893566082 && a.Count == 100073 && a.Ids == 10032092994);
  }
}

// Creatures on threads of their own, blocked in their sends and receives as the .prg's (:Wait(), :Result).
static class ChameneosReduxBench {
  const int Blue = 0, Red = 1, Yellow = 2;

  sealed record Request(int Id, int Color, Channel<int[]> Reply);

  static int Complement(int a, int b) => a == b ? a : 3 - a - b;

  // Whether n meetings of creatures of these colors count 2n meetings, none with itself. The caller is the broker;
  // the answer to a request is the other's { name, color }, or null once no meeting is left.
  static bool RunMeetings(int n, int[] colors) {
    var meet = Channel.CreateUnbounded<Request>();
    var threads = new Thread[colors.Length];
    var results = new long[colors.Length][];
    int left = n, done = 0;
    long met = 0, same = 0;
    Request first = null;
    for (int i = 0; i < colors.Length; i++) {
      int id = i + 1, color = colors[i], slot = i;
      threads[i] = new Thread(() => results[slot] = Live(meet, id, color));
      threads[i].Start();
    }
    while (done < colors.Length) {
      Request req = meet.Reader.ReadAsync().AsTask().Result;
      if (left == 0) {
        req.Reply.Writer.WriteAsync(null).AsTask().Wait();
        done++;
      } else if (first == null)
        first = req;
      else {
        first.Reply.Writer.WriteAsync(new[] { req.Id, req.Color }).AsTask().Wait();
        req.Reply.Writer.WriteAsync(new[] { first.Id, first.Color }).AsTask().Wait();
        first = null;
        left--;
      }
    }
    for (int i = 0; i < threads.Length; i++) {
      threads[i].Join();
      met += results[i][0];
      same += results[i][1];
    }
    return met == 2 * n && same == 0;
  }

  // A creature: asks for meetings until none is left; returns { meetings, meetings with itself }.
  static long[] Live(Channel<Request> meet, int id, int color) {
    var reply = Channel.CreateUnbounded<int[]>();
    long met = 0, same = 0;
    while (true) {
      meet.Writer.WriteAsync(new Request(id, color, reply)).AsTask().Wait();
      int[] other = reply.Reader.ReadAsync().AsTask().Result;
      if (other == null) break;
      color = Complement(color, other[1]);
      met++;
      if (other[0] == id) same++;
    }
    return new[] { met, same };
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    string table = "";
    for (int a = 0; a <= 2; a++)
      for (int b = 0; b <= 2; b++) table += Complement(a, b);
    bool ok = table == "021210102";
    ok = RunMeetings(30000, new[] { Blue, Red, Yellow }) && ok;
    ok = RunMeetings(30000, new[] { Blue, Red, Yellow, Red, Yellow, Blue, Red, Yellow, Red, Blue }) && ok;
    Program.Report("chameneos_redux", sw, ok);
  }
}
