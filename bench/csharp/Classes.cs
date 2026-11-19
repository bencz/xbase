// bench/method_call_overhead, virtual_dispatch, object_churn, object_creation, inheritance_depth, property_access,
// short_lived_objects and memory_footprint .prg in C#: the same work and checks. The .prg's classes are classes, its
// methods virtual where a subclass overrides them; an object the .prg makes by a macro (its class unknown to the
// compiler) comes from a factory the JIT does not see into, the calls the .prg sends through its cache made through
// an interface or a base class.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;

static class MethodCallOverheadBench {
  interface ICalc {
    long Zero();
    long One(long x);
    long Three(long x, long y, long z);
    long Twice(long x);
    long Chain(long x);
  }

  sealed class TCalc : ICalc {
    long nBase = 1;
    public long Zero() => nBase;
    public long One(long x) => x;
    public long Three(long x, long y, long z) => x + y + z - 5;
    public long Twice(long x) => x + x;
    public long Chain(long x) => One(x) + One(x) + 1;
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static ICalc ByName() => new TCalc();

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var o = new TCalc();
    ICalc u = ByName();
    long n = 0;
    for (long i = 1; i <= 5000000; i++) {
      n += o.Zero() + o.One(i) + o.Three(i, 2, 3) + o.Twice(i) + o.Chain(i);
      n += u.Zero() + u.One(i) + u.Three(i, 2, 3) + u.Twice(i) + u.Chain(i);
    }
    // each round adds 2 * ( 6 * i + 2 )
    Program.Report("method_call_overhead", sw, n == 150000050000000);
  }
}

static class VirtualDispatchBench {
  class TShape {
    protected long x, y;
    public TShape New(long x, long y) {
      this.x = x;
      this.y = y;
      return this;
    }
    public virtual long Area() => 0;
    public virtual long Perimeter() => 0;
    public long Measure() => Area() + 1;
  }

  sealed class TCircle : TShape {
    public override long Area() => 3 * x * x;
    public override long Perimeter() => 6 * x;
  }

  sealed class TSquare : TShape {
    public override long Area() => x * x;
    public override long Perimeter() => 4 * x;
  }

  sealed class TRect : TShape {
    public override long Area() => x * y;
    public override long Perimeter() => 2 * (x + y);
  }

  sealed class TTriangle : TShape {
    public override long Area() => x * y * 2 / 2;
    public override long Perimeter() => 3 * x;
  }

  sealed class THexagon : TShape {
    public override long Area() => 6 * x * x;
    public override long Perimeter() => 6 * x;
  }

  sealed class TEllipse : TShape {
    public override long Area() => 3 * x * y;
    public override long Perimeter() => 3 * (x + y);
  }

  static readonly MinStd random = new MinStd(6502);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    var shapes = new TShape[1000];
    Func<TShape>[] classes = { () => new TCircle(), () => new TSquare(), () => new TRect(), () => new TTriangle(),
                               () => new THexagon(), () => new TEllipse() };
    for (int i = 0; i < shapes.Length; i++) {
      TShape o = classes[Rand(6)]();
      long x = Rand(100) + 1;
      shapes[i] = o.New(x, Rand(100) + 1);
    }
    var sw = Stopwatch.StartNew();
    long area = 0, per = 0, m = 0;
    for (int i = 1; i <= 1000; i++)
      foreach (TShape o in shapes) {
        area += o.Area();
        per += o.Perimeter();
        m += o.Measure();
      }
    Program.Report("virtual_dispatch", sw, area == 1000L * 7844067 && per == 1000L * 245962 &&
                                           m == 1000L * (7844067 + 1000));
  }
}

static class ObjectChurnBench {
  class TLeaf {
    public long nValue;
    public TLeaf(long n) { nValue = n; }
    public virtual long Weight() => nValue;
  }

  sealed class TPair : TLeaf {
    readonly TLeaf oLeft, oRight;
    public TPair(long n, TLeaf l, TLeaf r) : base(n) {
      oLeft = l;
      oRight = r;
    }
    public override long Weight() => nValue + oLeft.nValue + oRight.nValue;
  }

  // Its items an array of three values, as the .prg's { o, n, Str( n ) } (n a remainder: Str() gives two decimals).
  sealed class TList : TLeaf {
    readonly object[] aItems;
    public TList(long n, TLeaf o) : base(n) { aItems = new object[] { o, n, XbNum.Str2(n) }; }
    public override long Weight() => nValue + ((TLeaf)aItems[0]).nValue + ((string)aItems[2]).Length;
  }

  static readonly MinStd random = new MinStd(9);
  static int Rand(int n) => (int)random.Next(n);

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var slots = new TLeaf[50000];
    long n = 0;
    for (int i = 0; i < slots.Length; i++) slots[i] = new TLeaf(i + 1);
    for (int i = 1; i <= 700000; i++) {
      int k = Rand(slots.Length);
      int kind = Rand(3);
      TLeaf o;
      if (kind == 0)
        o = new TLeaf(i % 100);
      else if (kind == 1) {
        TLeaf l = slots[Rand(slots.Length)];
        o = new TPair(i % 100, l, slots[Rand(slots.Length)]);
      } else
        o = new TList(i % 100, slots[Rand(slots.Length)]);
      slots[k] = o;
    }
    foreach (TLeaf o in slots) n += o.Weight();
    Program.Report("object_churn", sw, n == 5162688);
  }
}

static class ObjectCreationBench {
  class TOrder {
    public long nId, nTotal = 0;
    public string cCustomer = "", cStatus = "new";
    public List<long> aLines = new List<long>();
    public TOrder(long id, string customer) {
      nId = id;
      cCustomer = customer;
    }
  }

  sealed class TBigOrder : TOrder {
    public long nDiscount;
    public DateTime dDue = new DateTime(2026, 12, 31);
    public TBigOrder(long id, string customer) : base(id, customer) {}
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var keep = new List<TOrder>();
    long n = 0;
    for (int i = 1; i <= 600000; i++) {
      TOrder o;
      if (i % 2 == 0)
        o = new TOrder(i, "c" + (i % 1000).ToString("D3"));
      else
        o = new TBigOrder(i, "c000") { nDiscount = 5 };
      o.aLines.Add(i % 7);
      if (i % 100 == 0) keep.Add(o);
    }
    foreach (TOrder o in keep) n += o.nId + o.nTotal + o.aLines.Count + o.aLines[0] + o.cCustomer.Length;
    // the kept ones: ids 100, 200, ..., 600,000 (all even: TOrder), nTotal 0, one line of i % 7, a 4-character name;
    // 17999 is the sum of i % 7 over those ids
    Program.Report("object_creation", sw, keep.Count == 6000 && n == 1800300000 + 6000 * 5 + 17999);
  }
}

static class InheritanceDepthBench {
  class TL0 {
    public long v0;
    public virtual long Depth() => 1;
    public long Base() => v0 + 100;
  }

  class TL1 : TL0 {
    public long v1 = 1;
    public override long Depth() => base.Depth() + 2;
  }

  class TL2 : TL1 {
    public long v2 = 2;
    public override long Depth() => base.Depth() + 3;
  }

  class TL3 : TL2 {
    public long v3 = 3;
    public override long Depth() => base.Depth() + 4;
  }

  class TL4 : TL3 {
    public long v4 = 4;
    public override long Depth() => base.Depth() + 5;
  }

  class TL5 : TL4 {
    public long v5 = 5;
    public override long Depth() => base.Depth() + 6;
  }

  class TL6 : TL5 {
    public long v6 = 6;
    public override long Depth() => base.Depth() + 7;
  }

  class TL7 : TL6 {
    public long v7 = 7;
    public override long Depth() => base.Depth() + 8;
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static TL7 ByName() => new TL7();

  public static void Run() {
    var sw = Stopwatch.StartNew();
    TL7 o = ByName();
    long n = 0;
    for (long i = 1; i <= 4000000; i++) {
      n += o.Depth() + o.Base() + o.v0 + o.v4 + o.v7;
      o.v0 = i % 3;
    }
    // o.Depth() is 8 + 7 + ... + 1 = 36, o.Base() is o.v0 + 100, o.v4 + o.v7 is 11; o.v0 is ( i - 1 ) % 3, 0 at first
    Program.Report("inheritance_depth", sw, n == 4000000L * (36 + 100 + 11) + 2 * 3999999);
  }
}

static class PropertyAccessBench {
  class TAccount {
    public long nBalance, nCount, nLimit;
    public string cLast = "";
    public long Doubled => nBalance * 2 - nCount - 2;
    public long Limit {
      set => nLimit = value * 10;
    }
  }

  [MethodImpl(MethodImplOptions.NoInlining)]
  static TAccount ByName() => new TAccount();

  public static void Run() {
    var sw = Stopwatch.StartNew();
    TAccount o = ByName();
    long n = 0;
    for (long i = 1; i <= 3000000; i++) {
      o.nBalance += 2;
      o.nCount++;
      o.cLast = "d";
      n += o.nBalance - o.nCount + o.Doubled;
      o.Limit = i % 10;
      o.nBalance -= 1; // WITH OBJECT o
      n += o.nLimit + o.cLast.Length;
    }
    // round i reads nBalance i + 1, nCount i, Doubled i, then nLimit 10 * ( i % 10 ) and Len( cLast ) 1: it adds
    // i + 1 + 10 * ( i % 10 ) + 1
    Program.Report("property_access", sw, n == 4500001500000 + 10 * 13500000L + 2 * 3000000 && o.nBalance == 3000000);
  }
}

static class ShortLivedObjectsBench {
  sealed class TVec {
    public readonly long x, y;
    public TVec(long nX, long nY) {
      x = nX;
      y = nY;
    }
    public TVec Add(TVec o) => new TVec(x + o.x, y + o.y);
    public TVec Scaled(long k) => new TVec(x * k, y * k);
    public long Dot(TVec o) => x * o.x + y * o.y;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var s = new TVec(0, 0);
    long n = 0;
    for (long i = 1; i <= 2000000; i++) {
      var v = new TVec(i % 10, i % 7 - 3);
      s = s.Add(v);
      n += v.Dot(s.Scaled(0)) + v.x;
    }
    Program.Report("short_lived_objects", sw, s.x == 9000000 && s.y == -3 && n == 9000000);
  }
}

// The heap's bytes after full collections (GC.GetTotalMemory( true )), against the .prg's budgets per element. .NET
// counts no live objects: the .prg's checks of their number per element are left out.
static class MemoryFootprintBench {
  sealed class TPoint {
    public long x, y;
    public string cTag = "point";
    public TPoint New(long nX, long nY) {
      x = nX;
      y = nY;
      return this;
    }
  }

  static long Heap() => GC.GetTotalMemory(true);

  // Whether the n elements of x take within nBytes heap bytes each.
  static bool Within(object x, int count, int n, long nBytes, long heap) {
    long used = Heap() - heap;
    GC.KeepAlive(x);
    return count == n && used <= n * nBytes;
  }

  static bool Integers(long heap) {
    var a = new long[1000000];
    for (int i = 0; i < a.Length; i++) a[i] = (i + 1) * 3L;
    return Within(a, a.Length, 1000000, 24, heap);
  }

  static bool Objects(long heap) {
    var a = new TPoint[200000];
    for (int i = 0; i < a.Length; i++) a[i] = new TPoint().New(i + 1, -(i + 1));
    return Within(a, a.Length, 200000, 128, heap);
  }

  static bool Keys(long heap) {
    var h = new Dictionary<string, long>();
    for (int i = 1; i <= 300000; i++) h["k" + i.ToString().PadLeft(7)] = i;
    return Within(h, h.Count, 300000, 224, heap);
  }

  static bool Records(long heap) {
    var a = new object[600000][];
    for (int i = 0; i < a.Length; i++) a[i] = new object[] { (long)(i + 1), "item" + (i + 1).ToString().PadLeft(7) };
    return Within(a, a.Length, 600000, 224, heap);
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    long heap = Heap();
    bool ok = Integers(heap) && Objects(heap) && Keys(heap) && Records(heap);
    ok = ok && Heap() <= heap + 16 * 1024 * 1024;
    Program.Report("memory_footprint", sw, ok);
  }
}
