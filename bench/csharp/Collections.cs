// bench/hash_massivo, hash_chaves_longas, hash_resize, lru_cache, trie, array_growth, array_append, array_nested,
// array_of_hashes, array_preallocate, join_arrays, set_operations and linked_list .prg in C#: the same work and checks.
// Hashes are Dictionary (a set HashSet where the .prg's keys are their own values), arrays that grow List, records
// the .prg keeps in arrays small classes.
using System;
using System.Collections.Generic;
using System.Diagnostics;

static class HashMassivoBench {
  static readonly MinStd random = new MinStd(2024);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var h = new Dictionary<long, long>();
    long hits = 0, sum = 0;
    for (int i = 1; i <= 1000000; i++) {
      long k = Rand(1000000);
      if (h.TryGetValue(k, out long c))
        h[k] = c + 1;
      else
        h[k] = 1;
    }
    for (int i = 1; i <= 1500000; i++) {
      long k = Rand(2000000);
      if (h.TryGetValue(k, out long c)) {
        hits++;
        sum += c;
      }
    }
    Program.Report("hash_massivo", sw, h.Count == 631967 && hits == 473472 && sum == 748930);
  }
}

static class HashChavesLongasBench {
  static readonly MinStd random = new MinStd(99);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    const string prefix = "https://example.com/catalog/products/category/electronics/item?id=";
    int count = 300000;
    var keys = new string[count];
    var same = new string[count];
    var miss = new string[count];
    for (int i = 1; i <= count; i++) {
      long n = Rand(1000000);
      string id = i.ToString("D7"), v = n.ToString("D6"), x = new string('x', i % 50);
      keys[i - 1] = prefix + id + "&v=" + v + x;
      same[i - 1] = prefix + id + "&v=" + v + x;
      miss[i - 1] = prefix + id + "&w=" + v + x;
    }
    var sw = Stopwatch.StartNew();
    var h = new Dictionary<string, long>();
    long sum = 0, nMiss = 0;
    for (int i = 0; i < count; i++) h[keys[i]] = i + 1;
    for (int i = count - 1; i >= 0; i--) sum += h[same[i]];
    foreach (string k in miss)
      if (!h.ContainsKey(k)) nMiss++;
    Program.Report("hash_chaves_longas", sw, h.Count == 300000 && sum == 45000150000 && nMiss == 300000);
  }
}

static class HashResizeBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    long sum = 0;
    bool ok = true;
    for (long r = 1; r <= 20; r++) {
      var h = new Dictionary<long, long>();
      for (long i = 1; i <= 100000; i++) h[i * 7 + r] = i;
      ok = ok && h.Count == 100000;
      for (long i = 2; i <= 100000; i += 2) {
        sum += h[i * 7 + r];
        h.Remove(i * 7 + r);
      }
      ok = ok && h.Count == 50000;
      for (long i = 1; i <= 100000; i += 2) {
        sum += h[i * 7 + r];
        h.Remove(i * 7 + r);
      }
      ok = ok && h.Count == 0;
    }
    Program.Report("hash_resize", sw, ok && sum == 100001000000);
  }
}

// The list written out as the .prg writes it, its nodes objects of { key, value, prev, next }.
static class LruCacheBench {
  sealed class Node {
    public long Key, Value;
    public Node Prev, Next;
  }

  static readonly MinStd random = new MinStd(1234);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var h = new Dictionary<long, Node>();
    Node head = new Node(), tail = new Node();
    long hits = 0, sum = 0;
    int cap = 2000;
    head.Next = tail;
    tail.Prev = head;
    for (int i = 1; i <= 2000000; i++) {
      long k = Rand(Rand(20000) + 1);
      if (h.TryGetValue(k, out Node node)) {
        hits++;
        node.Prev.Next = node.Next;
        node.Next.Prev = node.Prev;
      } else {
        if (h.Count >= cap) {
          node = tail.Prev;
          node.Prev.Next = tail;
          tail.Prev = node.Prev;
          h.Remove(node.Key);
        }
        node = new Node { Key = k, Value = k * 3 + 1 };
        h[k] = node;
      }
      node.Next = head.Next;
      node.Prev = head;
      head.Next.Prev = node;
      head.Next = node;
      sum += node.Value;
    }
    Program.Report("lru_cache", sw, hits == 378837 && sum == 30033837356 && h.Count == cap);
  }
}

static class TrieBench {
  sealed class Node {
    public readonly Dictionary<char, Node> Children = new Dictionary<char, Node>();
    public long Words;
  }

  static readonly MinStd random = new MinStd(2718);
  static int Rand(int n) => (int)random.Next(n);

  static string NewWord() {
    int n = Rand(8) + 3;
    var w = new System.Text.StringBuilder();
    for (int i = 1; i <= n; i++) w.Append("etaoinsr"[Rand(Rand(8) + 1)]);
    return w.ToString();
  }

  public static void Run() {
    var words = new string[100000];
    var probe = new string[100000];
    for (int i = 0; i < words.Length; i++) {
      words[i] = NewWord();
      probe[i] = NewWord();
    }
    var sw = Stopwatch.StartNew();
    var root = new Node();
    long nodes = 1, found = 0, prefix = 0;
    foreach (string w in words) {
      Node node = root;
      foreach (char c in w) {
        if (!node.Children.TryGetValue(c, out Node next)) {
          next = new Node();
          node.Children[c] = next;
          nodes++;
        }
        node = next;
      }
      node.Words++;
    }
    foreach (string w in probe) {
      Node node = root;
      foreach (char c in w)
        if (!node.Children.TryGetValue(c, out node)) break;
      if (node != null) {
        prefix++;
        if (node.Words > 0) found++;
      }
    }
    Program.Report("trie", sw, nodes == 159557 && found == 40767 && prefix == 50841);
  }
}

// The arrays are List<long>: ASize, AIns and ADel as the list's resizing, Insert and RemoveAt (NIL as 0, never read).
static class ArrayGrowthBench {
  static readonly MinStd random = new MinStd(3);
  static int Rand(int n) => (int)random.Next(n);

  static void ASize(List<long> a, int n) {
    if (n < a.Count)
      a.RemoveRange(n, a.Count - n);
    else
      while (a.Count < n) a.Add(0);
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var kept = new List<List<long>>();
    long n = 0;
    for (int i = 1; i <= 300000; i++) {
      var a = new List<long>();
      int size = Rand(64) + 1;
      for (int j = 1; j <= size; j++) a.Add(j * 2);
      ASize(a, size * 2);
      a[size * 2 - 1] = i % 1000;
      n += a[size * 2 - 1];
      ASize(a, (size + 1) / 2);
      a.Insert(0, 0); // AIns( a, 1 )
      a.RemoveAt(a.Count - 1);
      a[0] = size;
      a.RemoveAt(0); // ADel( a, 1 )
      a.Add(0);
      a[^1] = i % 7;
      n += a.Count + a[^1] + (a.Count > 1 ? a[0] : 0);
      if (i % 100 == 0) kept.Add(a);
    }
    foreach (var a in kept) n += a[0];
    Program.Report("array_growth", sw, n == 156283982 && kept.Count == 3000);
  }
}

static class ArrayAppendBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    long sum = 0;
    List<long> a = null;
    for (int r = 1; r <= 5; r++) {
      a = new List<long>();
      for (int i = 1; i <= 2000000; i++) a.Add(i + r);
      foreach (long x in a) sum += x;
    }
    Program.Report("array_append", sw, sum == 10000035000000 && a.Count == 2000000);
  }
}

static class ArrayNestedBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    var a = new long[100][][];
    long sum = 0;
    for (int i = 1; i <= 100; i++) {
      a[i - 1] = new long[100][];
      for (int j = 1; j <= 100; j++) {
        a[i - 1][j - 1] = new long[100];
        for (int k = 1; k <= 100; k++) a[i - 1][j - 1][k - 1] = (i * 31 + j * 17 + k * 7) % 101;
      }
    }
    for (int p = 1; p <= 10; p++) {
      for (int i = 0; i < 100; i++) {
        long[][] plane = a[i];
        for (int j = 1; j < 100; j++) {
          long[] row = plane[j];
          for (int k = 1; k < 100; k++) row[k] = (row[k] + plane[j - 1][k] + row[k - 1]) % 1000;
        }
      }
      for (int i = 0; i < 100; i++)
        for (int j = 0; j < 100; j++)
          for (int k = 0; k < 100; k++) sum += a[i][j][k];
    }
    Program.Report("array_nested", sw, sum == 4905893027);
  }
}

// Each record a Dictionary<string, object>, its fields read by name.
static class ArrayOfHashesBench {
  static readonly MinStd random = new MinStd(31337);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var a = new Dictionary<string, object>[120000];
    var total = new long[51];
    long pass = 0, sum = 0;
    for (int i = 0; i < a.Length; i++) {
      var r = new Dictionary<string, object> { ["id"] = (long)(i + 1) };
      r["name"] = "N" + Rand(100000).ToString("D5");
      r["score"] = Rand(1000);
      r["group"] = Rand(50) + 1;
      a[i] = r;
    }
    foreach (var r in a) {
      total[(long)r["group"]] += (long)r["score"];
      if ((long)r["score"] >= 500 && ((string)r["name"]).StartsWith("N1", StringComparison.Ordinal)) pass++;
    }
    Array.Sort(a, (x, y) => ((long)x["score"]).CompareTo((long)y["score"]));
    for (int i = 0; i < a.Length; i += 1000) sum += (long)a[i]["score"];
    bool ok = total[1] == 1239678 && total[50] == 1170281 && pass == 5950 && sum == 59429 &&
              (long)a[0]["score"] == 0 && (long)a[119999]["score"] == 999;
    Program.Report("array_of_hashes", sw, ok);
  }
}

static class ArrayPreallocateBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 2000000;
    long sum = 0;
    long[] a = null;
    for (int r = 1; r <= 25; r++) {
      a = new long[n];
      for (int i = 0; i < n; i++) a[i] = i + 1 + r;
      for (int i = 0; i < n; i++) sum += a[i];
    }
    Program.Report("array_preallocate", sw, sum == 50000675000000 && a.Length == n);
  }
}

static class JoinArraysBench {
  sealed class Customer {
    public long Id, Segment;
    public string Name, City;
  }

  sealed class Order {
    public long Id, Customer, Amount;
  }

  static readonly MinStd random = new MinStd(2718);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    var cust = new Customer[20000];
    var ord = new Order[500000];
    for (int i = 0; i < cust.Length; i++) {
      var c = new Customer { Id = (i + 1) * 3L, Name = "Customer " + (i + 1) };
      c.Segment = Rand(5) + 1;
      c.City = "CITY" + (Rand(100) + 1).ToString("D3");
      cust[i] = c;
    }
    for (int i = 0; i < ord.Length; i++) {
      var o = new Order { Id = i + 1 };
      o.Customer = Rand(60000) + 1;
      o.Amount = Rand(100000) + 1;
      ord[i] = o;
    }
    var sw = Stopwatch.StartNew();
    var hCust = new Dictionary<long, Customer>();
    var seg = new long[6];
    var hCity = new Dictionary<string, long>();
    long matched = 0, w = 0, merged = 0, best = 0;
    string cityBest = null;
    foreach (var c in cust) hCust[c.Id] = c;
    foreach (var o in ord)
      if (hCust.TryGetValue(o.Customer, out Customer c)) {
        seg[c.Segment] += o.Amount;
        hCity[c.City] = hCity.GetValueOrDefault(c.City) + 1;
        matched++;
      }
    foreach (var (k, n) in hCity)
      if (n > best || (n == best && string.CompareOrdinal(k, cityBest) < 0)) {
        cityBest = k;
        best = n;
      }

    var keys = new long[ord.Length];
    for (int i = 0; i < ord.Length; i++) keys[i] = ord[i].Customer * 1000000 + i + 1;
    Array.Sort(keys);
    int pos = 0;
    foreach (long k in keys) {
      Order o = ord[k % 1000000 - 1];
      while (pos < cust.Length && cust[pos].Id < o.Customer) pos++;
      if (pos < cust.Length && cust[pos].Id == o.Customer) {
        w += o.Amount * cust[pos].Segment;
        merged++;
      }
    }

    bool ok = matched == 166641 && merged == 166641 && w == 25122891088 && seg[1] == 1626943956 &&
              seg[2] == 1648544230 && seg[3] == 1673211091 && seg[4] == 1694374411 && seg[5] == 1680345551 &&
              cityBest == "CITY080" && best == 1980 && hCity.Count == 100;
    Program.Report("join_arrays", sw, ok);
  }
}

static class SetOperationsBench {
  static readonly MinStd random = new MinStd(777);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    var sw = Stopwatch.StartNew();
    long check = 0;
    HashSet<long> a = null, union = null, inter = null, diff = null, sym = null;
    for (int r = 1; r <= 2; r++) {
      a = new HashSet<long>();
      var b = new HashSet<long>();
      for (int i = 1; i <= 200000; i++) {
        a.Add(Rand(400000));
        b.Add(Rand(400000));
      }
      union = new HashSet<long>();
      inter = new HashSet<long>();
      diff = new HashSet<long>();
      sym = new HashSet<long>();
      foreach (long k in a) {
        union.Add(k);
        if (b.Contains(k))
          inter.Add(k);
        else {
          diff.Add(k);
          sym.Add(k);
        }
      }
      foreach (long k in b) {
        union.Add(k);
        if (!a.Contains(k)) sym.Add(k);
      }
      check += a.Count + b.Count * 2 + union.Count * 3 + inter.Count * 5 + diff.Count * 7 + sym.Count * 11;
    }
    bool ok = check == 8609885 && union.Count == 252664 && union.Count == inter.Count + sym.Count &&
              a.Count == inter.Count + diff.Count;
    Program.Report("set_operations", sw, ok);
  }
}

static class LinkedListBench {
  sealed class Node {
    public long Value;
    public Node Next;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    long sum = 0, count = 0, first = 0;
    for (int r = 1; r <= 3; r++) {
      Node head = null, node, prev;
      for (int i = 1; i <= 1000000; i++) head = new Node { Value = i, Next = head };
      for (node = head; node != null; node = node.Next) sum += node.Value;
      prev = null;
      node = head;
      while (node != null) {
        Node next = node.Next;
        node.Next = prev;
        prev = node;
        node = next;
      }
      head = prev;
      first += head.Value;
      node = head;
      while (node != null && node.Next != null) {
        node.Next = node.Next.Next;
        node = node.Next;
      }
      for (node = head; node != null; node = node.Next) count++;
    }
    Program.Report("linked_list", sw, sum == 1500001500000 && first == 3 && count == 1500000);
  }
}
