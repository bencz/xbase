// bench/oop, gc, bitlookup and generator .prg in C#: the same work and checks.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;

class TBase {
  public long total;
  public virtual void Add(long x) { total += x; }
}

class TCounter : TBase {
  public long n;
  public override void Add(long x) { base.Add(x); } // Super:Add( x )
  public long Result() => total + n;
}

static class Oop {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    TBase o = new TCounter();
    for (long i = 1; i <= 3000000; i++) {
      o.Add(i % 7);
      ((TCounter)o).n = ((TCounter)o).n + 1;
    }
    Program.Report("oop", sw, ((TCounter)o).Result() == 11999997);
  }
}

static class Gc {
  static ulong seed = 7;
  static long Rand(long n) {
    seed = (seed * 1103515245 + 12345) % 2147483648;
    return (long)(seed / 65536) % n;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var slots = new object[40000];
    var kept = new List<object[]>();
    for (long i = 1; i <= 3000000; i++) {
      long n = Rand(slots.Length);
      object x;
      if (Rand(4) == 0)
        x = new object[] { new string((char)(65 + i % 26), (int)(16 + Rand(200))), i, new string(' ', (int)Rand(100)) };
      else
        x = new string((char)(97 + i % 26), (int)(16 + Rand(2000)));
      slots[n] = x;
      if (i % 150 == 0) kept.Add(new object[] { i, i.ToString().PadLeft(10), new string('k', 64) });
    }
    long check = 0;
    foreach (var x in slots) {
      if (x is string s) check += s.Length;
      else if (x is object[] a) check += ((string)a[0]).Length + ((string)a[2]).Length;
    }
    Program.Report("gc", sw, kept.Count == 20000 && check > 0);
  }
}

static class BitLookup {
  static readonly ushort[] Table = MakeTable();
  static ushort[] MakeTable() {
    var t = new ushort[256];
    for (int a = 0; a < 16; a++)
      for (int b = 0; b < 16; b++) t[a * 16 + b] = (ushort)(a & b);
    return t;
  }

  // Not inlined: each of the 7 calls a pair is made, as xbase calls its function.
  [MethodImpl(MethodImplOptions.NoInlining)]
  static int And(int a, int b) {
    int r = 0;
    for (int i = 0; i < 16; i += 4) {
      r = r / 16 + Table[(a % 16) * 16 + (b % 16)] * 4096;
      a /= 16;
      b /= 16;
    }
    return r;
  }
  public static void Run() {
    var sw = Stopwatch.StartNew();
    bool ok = true;
    for (int a = 0; a < 1000 && ok; a++)
      for (int b = 0; b < 1000; b++)
        // IOR and XOR written out as the .prg's macros expand them.
        if (And(a, b) != (a & b) ||
            (a + b - 2 * And(a, b)) + And(a, b) - 2 * And(a + b - 2 * And(a, b), And(a, b)) != (a | b) ||
            a + b - 2 * And(a, b) != (a ^ b)) {
          ok = false;
          break;
        }
    Program.Report("bitlookup", sw, ok);
  }
}

static class Generator {
  static IEnumerable<long> Range(long a, long b) {
    for (long i = a; i <= b; i++) yield return i;
  }
  static IEnumerable<long> Evens(IEnumerable<long> e) {
    foreach (long x in e)
      if (x % 2 == 0) yield return x;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    long s = 0;
    foreach (long x in Evens(Range(1, 3000000))) s += x;
    Program.Report("generator", sw, s == 2250001500000);
  }
}
