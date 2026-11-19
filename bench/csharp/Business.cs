// bench/relatorio_agregacao, pivot_crosstab, sequential_scan, index, json_roundtrip, log_processing, validacao_dados,
// geracao_html, indexacao_simulada, date_time_ops and numeric_formatting .prg in C#: the same work and checks. Records
// the .prg keeps in arrays are small classes, its hashes Dictionary (OrderedDictionary where the order of the keys is
// written out), its dates DateTime; numbers are formatted by xbase's rules (Xbase.cs). The tables of sequential_scan
// and index are files of fixed-width records the program writes and deletes (C# has no DBF).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Text;

// A table in a file of fixed-width records, the fields as text as a DBF keeps them (a deletion flag, then each
// field), read and written through a buffer of one record.
sealed class RecordFile : IDisposable {
  readonly FileStream file;
  readonly string path;
  public readonly byte[] Record;
  public int Count, RecNo;

  public RecordFile(string path, int length) {
    this.path = path;
    file = new FileStream(path, FileMode.Create, FileAccess.ReadWrite, FileShare.None, 65536);
    Record = new byte[length];
  }

  // APPEND BLANK: a record of blanks at the end, current.
  public void AppendBlank() {
    Record.AsSpan().Fill((byte)' ');
    RecNo = ++Count;
  }

  // Writes the current record.
  public void Commit() {
    MoveTo(RecNo);
    file.Write(Record);
  }

  public void Go(int recNo) {
    RecNo = recNo;
    MoveTo(recNo);
    file.ReadExactly(Record);
  }

  // Positions the file at a record, without a seek (which drops the stream's buffer) when it is there already.
  void MoveTo(int recNo) {
    long at = (long)(recNo - 1) * Record.Length;
    if (file.Position != at) file.Position = at;
  }

  // GO TOP, then SKIP: the records in turn, false past the last.
  public void GoTop() {
    file.Flush();
    file.Position = 0;
    RecNo = 0;
  }

  public bool Skip() {
    if (RecNo >= Count) return false;
    file.ReadExactly(Record);
    RecNo++;
    return true;
  }

  public void PutNum(int offset, int width, long value) =>
    Encoding.ASCII.GetBytes(value.ToString(CultureInfo.InvariantCulture).PadLeft(width), Record.AsSpan(offset, width));

  public void PutText(int offset, int width, string value) =>
    Encoding.ASCII.GetBytes(value.PadRight(width), Record.AsSpan(offset, width));

  public void PutDate(int offset, DateTime value) =>
    PutText(offset, 8, value.ToString("yyyyMMdd", CultureInfo.InvariantCulture));

  public long Num(int offset, int width) {
    long v = 0;
    foreach (byte b in Record.AsSpan(offset, width))
      if (b != ' ') v = v * 10 + (b - '0');
    return v;
  }

  public string Text(int offset, int width) => Encoding.ASCII.GetString(Record, offset, width);

  public DateTime Date(int offset) =>
    new DateTime((int)Num(offset, 4), (int)Num(offset + 4, 2), (int)Num(offset + 6, 2));

  public void Dispose() {
    file.Dispose();
    File.Delete(path);
  }
}

static class RelatorioAgregacaoBench {
  sealed class Sale {
    public string Region, Product;
    public long Qty, Price;
  }

  static readonly MinStd random = new MinStd(12345);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    var sales = new Sale[1000000];
    for (int i = 0; i < sales.Length; i++) {
      var s = new Sale { Region = "R" + (Rand(8) + 1).ToString("D2") };
      s.Product = "P" + (Rand(500) + 1).ToString("D4");
      s.Qty = Rand(20) + 1;
      s.Price = Rand(100000) + 100;
      sales[i] = s;
    }
    var sw = Stopwatch.StartNew();
    var hReg = new Dictionary<string, long[]>();
    var hProd = new Dictionary<string, long>();
    long total = 0;
    foreach (Sale r in sales) {
      long x = r.Qty * r.Price;
      if (hReg.TryGetValue(r.Region, out long[] a)) {
        a[0]++;
        a[1] += r.Qty;
        a[2] += x;
      } else
        hReg[r.Region] = new[] { 1, r.Qty, x };
      hProd[r.Product] = hProd.GetValueOrDefault(r.Product) + x;
      total += x;
    }
    var prod = new List<KeyValuePair<string, long>>(hProd);
    prod.Sort((p, q) => p.Value != q.Value ? q.Value.CompareTo(p.Value) : string.CompareOrdinal(p.Key, q.Key));
    var regions = new List<string>(hReg.Keys); // HSort( hReg )
    regions.Sort(string.CompareOrdinal);
    var rep = new StringBuilder();
    foreach (string k in regions) {
      long[] a = hReg[k];
      rep.Append(k).Append(XbNum.Transform(a[0], "@E 9,999,999")).Append(XbNum.Transform(a[1], "@E 99,999,999"))
         .Append(XbNum.Transform(a[2] / 100.0, "@E 999,999,999,999.99")).Append('\n');
    }
    for (int i = 0; i < 20; i++)
      rep.Append(prod[i].Key).Append(XbNum.Transform(prod[i].Value / 100.0, "@E 999,999,999,999.99")).Append('\n');
    string c = rep.ToString();
    string[] lines = c.Split('\n');
    bool ok = total == 525748851783 && c.Length == 8 * 41 + 20 * 24 && hProd.Count == 500 &&
              lines[0] == "R01  125.691 1.320.222    662.846.057,84" &&
              lines[7] == "R08  124.683 1.310.936    656.013.155,38" && lines[8] == "P0303     11.438.921,18" &&
              lines[27] == "P0360     11.021.303,35";
    Program.Report("relatorio_agregacao", sw, ok);
  }
}

static class PivotCrosstabBench {
  sealed class Sale {
    public string Seller;
    public DateTime Date;
    public long Amount;
  }

  static readonly MinStd random = new MinStd(16180);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    var sales = new Sale[2000000];
    var dBase = new DateTime(2025, 1, 1);
    for (int i = 0; i < sales.Length; i++) {
      var s = new Sale { Seller = "SELLER" + (Rand(60) + 1).ToString("D3") };
      s.Date = dBase.AddDays(Rand(365));
      s.Amount = Rand(50000) + 1;
      sales[i] = s;
    }
    var sw = Stopwatch.StartNew();
    var hRows = new Dictionary<string, long[]>();
    var col = new long[12];
    var dow = new long[7][];
    for (int i = 0; i < 7; i++) dow[i] = new long[12];
    foreach (Sale r in sales) {
      if (!hRows.TryGetValue(r.Seller, out long[] a)) {
        a = new long[12];
        hRows[r.Seller] = a;
      }
      int month = r.Date.Month;
      a[month - 1] += r.Amount;
      dow[(int)r.Date.DayOfWeek][month - 1] += r.Amount; // DoW() - 1: Sunday first
    }
    var sellers = new List<string>(hRows.Keys); // HSort( hRows )
    sellers.Sort(string.CompareOrdinal);
    var tab = new StringBuilder();
    string best = null, s07 = null;
    long bestRow = 0, grand = 0, sat = 0;
    foreach (string k in sellers) {
      long[] a = hRows[k];
      var line = new StringBuilder(k.PadRight(10));
      long row = 0;
      for (int i = 0; i < 12; i++) {
        line.Append(XbNum.Str(a[i], 11, 0));
        row += a[i];
        col[i] += a[i];
      }
      line.Append(XbNum.Str(row, 13, 0));
      if (row > bestRow) {
        best = k;
        bestRow = row;
      }
      if (k == "SELLER007") s07 = line.ToString();
      tab.Append(line).Append('\n');
    }
    var total = new StringBuilder("TOTAL".PadRight(10));
    for (int i = 0; i < 12; i++) {
      total.Append(XbNum.Str(col[i], 11, 0));
      grand += col[i];
    }
    tab.Append(total).Append(XbNum.Str(grand, 13, 0)).Append('\n');
    Array.ForEach(dow[6], x => sat += x);
    bool ok = grand == 50028051852 && best == "SELLER015" && bestRow == 844241139 && col[0] == 4275964399 &&
              col[1] == 3837132322 && col[11] == 4246237533 && dow[0][0] == 547728957 && sat == 7111132032 &&
              tab.Length == 61 * (10 + 12 * 11 + 13 + 1) &&
              s07 == "SELLER007 " + "   71382096   62515370   68284183   68152167   70863968   69201960" +
                     "   71447433   71672784   67917922   68878748   65718083   68477906    824512620";
    Program.Report("pivot_crosstab", sw, ok);
  }
}

// The table a file of fixed-width records (REGION N 1, AMOUNT N 10, QTY N 3, CUST C 10, SALEDATE D 8 after the
// deletion flag), each pass reading it from the start; SUM, COUNT, dbEval and LOCATE ... CONTINUE are loops.
static class SequentialScanBench {
  const int Region = 1, Amount = 2, Qty = 12, Cust = 15, SaleDate = 25, Length = 33;

  static readonly MinStd random = new MinStd(31337);
  static long Rand(long n) => random.Next(n);

  // dbEval( bBlock, bFor ): the block for each record for which bFor is true.
  static void DbEval(RecordFile t, Action block, Func<bool> condition) {
    t.GoTop();
    while (t.Skip())
      if (condition()) block();
  }

  // LOCATE FOR / CONTINUE: the next record from the current one for which the condition is true; false when none.
  static bool Locate(RecordFile t, Func<bool> condition) {
    while (t.Skip())
      if (condition()) return true;
    return false;
  }

  static bool Scans(RecordFile t) {
    long sumR3 = 0, cntR3 = 0, all = 0, big = 0, qty = 0, loc = 0, locIds = 0, jan = 0;
    t.GoTop();
    while (t.Skip())
      if (t.Num(Region, 1) == 3) {
        sumR3 += t.Num(Amount, 10);
        cntR3++;
      }
    t.GoTop(); // SUM AMOUNT TO nAll
    while (t.Skip()) all += t.Num(Amount, 10);
    t.GoTop(); // COUNT TO nBig FOR AMOUNT > 900000
    while (t.Skip())
      if (t.Num(Amount, 10) > 900000) big++;
    DbEval(t, () => qty += t.Num(Qty, 3), () => t.Num(Region, 1) <= 2);
    t.GoTop();
    while (Locate(t, () => t.Text(Cust, 10).Substring(0, 8) == "CUST0001")) {
      loc++;
      locIds += t.RecNo;
    }
    var february = new DateTime(2026, 2, 1);
    t.GoTop(); // SUM AMOUNT TO nJan FOR SALEDATE < SToD( "20260201" )
    while (t.Skip())
      if (t.Date(SaleDate) < february) jan += t.Num(Amount, 10);
    return sumR3 == 11099768125 && cntR3 == 22094 && all == 99898922935 && big == 19973 && qty == 1135702 &&
           loc == 396 && locIds == 40072280 && jan == 8435099403;
  }

  public static void Run() {
    var dBase = new DateTime(2026, 1, 1);
    bool ok = true;
    using var t = new RecordFile("bench_seqscan.dat", Length);
    for (int i = 1; i <= 200000; i++) {
      t.AppendBlank();
      t.PutNum(Region, 1, Rand(9) + 1);
      t.PutNum(Amount, 10, Rand(1000000));
      t.PutNum(Qty, 3, Rand(50) + 1);
      t.PutDate(SaleDate, dBase.AddDays(Rand(365)));
      t.PutText(Cust, 10, "CUST" + Rand(50000).ToString("D6"));
      t.Commit();
    }
    var sw = Stopwatch.StartNew();
    for (int i = 1; i <= 4; i++) ok = ok && Scans(t);
    Program.Report("sequential_scan", sw, ok);
  }
}

// The table a file of fixed-width records (K C 10, N N 10 after the deletion flag); the order a SortedSet of
// (key, record number) built from the table, kept up to date as records are appended. SEEK looks for the key's
// entries in it (the soft seek's position is not used); a walk through the order reads each record.
static class IndexBench {
  const int K = 1, N = 11, Length = 21;

  static long seed = 7;
  static long Rand(long n) {
    seed = (seed * 1103515245 + 12345) % 2147483648;
    return seed / 65536 % n;
  }

  static readonly IComparer<(string Key, int Rec)> ByKey =
    Comparer<(string Key, int Rec)>.Create((a, b) => {
      int c = string.CompareOrdinal(a.Key, b.Key);
      return c != 0 ? c : a.Rec.CompareTo(b.Rec);
    });

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int recs = 200000;
    long hits = 0, count = 0;
    bool ok = true;
    using (var t = new RecordFile("bench_index.dat", Length)) {
      for (int i = 1; i <= recs; i++) {
        t.AppendBlank();
        t.PutText(K, 10, Rand(100000000).ToString("D10"));
        t.PutNum(N, 10, i);
        t.Commit();
      }
      var keys = new List<(string Key, int Rec)>(recs); // INDEX ON K
      t.GoTop();
      while (t.Skip()) keys.Add((t.Text(K, 10), t.RecNo));
      var order = new SortedSet<(string Key, int Rec)>(keys, ByKey);
      for (int i = 1; i <= 100000; i++) {
        string key = Rand(100000000).ToString("D10");
        if (order.GetViewBetween((key, 0), (key, int.MaxValue)).Count > 0) hits++;
      }
      string prev = "";
      foreach (var e in order) {
        t.Go(e.Rec);
        string k = t.Text(K, 10);
        ok = ok && string.CompareOrdinal(k, prev) >= 0;
        prev = k;
        count++;
      }
      ok = ok && count == recs;
      for (int i = 1; i <= 20000; i++) {
        t.AppendBlank();
        string key = Rand(100000000).ToString("D10");
        t.PutText(K, 10, key);
        t.PutNum(N, 10, recs + i);
        t.Commit();
        order.Add((key, t.RecNo));
      }
      count = 0;
      foreach (var e in order) {
        t.Go(e.Rec);
        count++;
      }
    }
    ok = ok && count == recs + 20000 && hits >= 0;
    Program.Report("index", sw, ok);
  }
}

// The records are OrderedDictionary (the keys written in the order they were set), arrays List<object>.
static class JsonRoundtripBench {
  static readonly MinStd random = new MinStd(1618);
  static long Rand(long n) => random.Next(n);

  static string ToJson(object x) {
    switch (x) {
    case OrderedDictionary<string, object> h: {
      var c = new StringBuilder("{");
      string sep = "";
      foreach (var (k, v) in h) {
        string item = Quote(k) + ":" + ToJson(v);
        c.Append(sep).Append(item);
        sep = ",";
      }
      return c.Append('}').ToString();
    }
    case List<object> a: {
      var c = new StringBuilder("[");
      string sep = "";
      foreach (object v in a) {
        string item = ToJson(v);
        c.Append(sep).Append(item);
        sep = ",";
      }
      return c.Append(']').ToString();
    }
    case string s:
      return Quote(s);
    case long n:
      return n.ToString(CultureInfo.InvariantCulture);
    case bool b:
      return b ? "true" : "false";
    }
    return "null";
  }

  static string Quote(string c) =>
    "\"" + c.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n").Replace("\t", "\\t") + "\"";

  // The value that starts at pos, which it leaves after the value.
  static object Parse(string c, ref int pos) {
    char ch = c[pos];
    if (ch == '{') {
      var x = new OrderedDictionary<string, object>();
      pos++;
      if (c[pos] == '}') {
        pos++;
        return x;
      }
      while (true) {
        string k = ParseString(c, ref pos);
        pos++;
        x[k] = Parse(c, ref pos);
        if (c[pos++] == '}') return x;
      }
    }
    if (ch == '[') {
      var x = new List<object>();
      pos++;
      if (c[pos] == ']') {
        pos++;
        return x;
      }
      while (true) {
        x.Add(Parse(c, ref pos));
        if (c[pos++] == ']') return x;
      }
    }
    if (ch == '"') return ParseString(c, ref pos);
    if (ch == 't') {
      pos += 4;
      return true;
    }
    if (ch == 'f') {
      pos += 5;
      return false;
    }
    if (ch == 'n') {
      pos += 4;
      return null;
    }
    int start = pos;
    while (pos < c.Length && "-0123456789".Contains(c[pos])) pos++;
    return long.Parse(c.AsSpan(start, pos - start), CultureInfo.InvariantCulture);
  }

  static string ParseString(string c, ref int pos) {
    var output = new StringBuilder();
    int start = ++pos;
    char ch;
    while ((ch = c[pos]) != '"')
      if (ch == '\\') {
        output.Append(c, start, pos - start);
        ch = c[pos + 1];
        output.Append(ch == 'n' ? '\n' : ch == 't' ? '\t' : ch);
        pos += 2;
        start = pos;
      } else
        pos++;
    output.Append(c, start, pos - start);
    pos++;
    return output.ToString();
  }

  // A record, drawn from the seed in the order the checks' constants were computed.
  static OrderedDictionary<string, object> MakeRecord(long i) {
    const string chars = "abcdefghijk ABC\"\\\n\txyz";
    var name = new StringBuilder();
    long n = Rand(11) + 5;
    for (long k = 1; k <= n; k++) name.Append(chars[(int)Rand(22)]);
    n = Rand(4);
    bool active = Rand(2) == 1;
    long qty = Rand(2001) - 1000;
    object parent = null;
    if (Rand(3) != 0) parent = Rand(i) + 1;
    string city = "City " + Rand(50);
    long zip = Rand(90000) + 10000;
    var tags = new List<object>();
    for (long k = 1; k <= n; k++) tags.Add("tag" + Rand(100));
    return new OrderedDictionary<string, object> {
      ["id"] = i, ["name"] = name.ToString(), ["tags"] = tags, ["active"] = active, ["qty"] = qty, ["parent"] = parent,
      ["address"] = new OrderedDictionary<string, object> { ["city"] = city, ["zip"] = zip }
    };
  }

  public static void Run() {
    var data = new List<object>(20000);
    for (int i = 1; i <= 20000; i++) data.Add(MakeRecord(i));
    var sw = Stopwatch.StartNew();
    string c1 = ToJson(data);
    int pos = 0;
    var back = (List<object>)Parse(c1, ref pos);
    string c2 = ToJson(back);
    long qty = 0, active = 0, tags = 0, nulls = 0, zip = 0;
    foreach (OrderedDictionary<string, object> h in back) {
      qty += (long)h["qty"];
      active += (bool)h["active"] ? 1 : 0;
      tags += ((List<object>)h["tags"]).Count;
      nulls += h["parent"] == null ? 1 : 0;
      zip += (long)((OrderedDictionary<string, object>)h["address"])["zip"];
    }
    var first = (OrderedDictionary<string, object>)back[0];
    var second = (OrderedDictionary<string, object>)back[1];
    bool ok = c1 == c2 && c1.Length == 2697804 && pos == c1.Length && back.Count == 20000 && qty == -88759 &&
              active == 9960 && tags == 29729 && nulls == 6525 && zip == 1098378340 &&
              (string)first["name"] == "ykAeACczkccifkx" && (string)second["name"] == "Cdkk\"\\Ae\nx\nz" &&
              c1.StartsWith("[{\"id\":1,\"name\":\"ykAeACczkccifkx\"", StringComparison.Ordinal);
    Program.Report("json_roundtrip", sw, ok);
  }
}

static class LogProcessingBench {
  static readonly MinStd random = new MinStd(9001);
  static long Rand(long n) => random.Next(n);

  public static void Run() {
    string[] lv = { "INFO ", "WARN ", "ERROR", "DEBUG" }, mod = { "auth", "db", "http", "cache", "queue", "mail" };
    long[] st = { 200, 200, 200, 201, 204, 301, 404, 500, 503, 200 };
    int n = 300000;
    var text = new StringBuilder();
    for (long i = 1; i <= n; i++) {
      long sec = i * 86400 / n, x = Rand(1000), l = Rand(10);
      l = l < 6 ? 1 : l < 8 ? 4 : l < 9 ? 2 : 3;
      string line = "2026-11-19 " + (sec / 3600).ToString("D2") + ":" + (sec / 60 % 60).ToString("D2") + ":" +
                    (sec % 60).ToString("D2") + "." + x.ToString("D3") + " " + lv[l - 1] + " [" + mod[Rand(6)] +
                    "] request done user=u" + Rand(5000);
      line += " ms=" + Rand(2000);
      line += " status=" + st[Rand(10)].ToString().PadLeft(3);
      text.Append(line).Append('\n');
    }
    string c = text.ToString();
    var sw = Stopwatch.StartNew();
    var hLv = new Dictionary<string, long>();
    var hMod = new Dictionary<string, long>();
    var users = new HashSet<string>();
    var hourErr = new long[24];
    long count = 0, max = -1, maxLine = 0, err5 = 0;
    foreach (string x in c.Split('\n')) {
      if (string.IsNullOrWhiteSpace(x)) continue;
      count++;
      string l = x.Substring(24, 5).TrimEnd(' ');
      hLv[l] = hLv.GetValueOrDefault(l) + 1;
      int open = x.IndexOf('['), ms = x.IndexOf(" ms=", StringComparison.Ordinal);
      string cMod = x.Substring(open + 1, x.IndexOf(']') - open - 1);
      long nMs = (long)XbNum.Val(x.Substring(ms + 4));
      hMod[cMod] = hMod.GetValueOrDefault(cMod) + nMs;
      if (nMs > max) {
        max = nMs;
        maxLine = count;
      }
      long nSt = (long)XbNum.Val(x.Substring(x.IndexOf(" status=", StringComparison.Ordinal) + 8));
      if (nSt >= 500) {
        err5++;
        hourErr[(long)XbNum.Val(x.Substring(11, 2))]++;
      }
      int user = x.IndexOf(" user=", StringComparison.Ordinal);
      users.Add(x.Substring(user + 6, ms - user - 6));
    }
    bool ok = c.Length == 23765927 && count == n && hLv["INFO"] == 179990 && hLv["WARN"] == 29916 &&
              hLv["ERROR"] == 30173 && hLv["DEBUG"] == 59921 && hMod["auth"] == 50005329 && hMod["db"] == 50599071 &&
              hMod["http"] == 50094170 && hMod["cache"] == 49851623 && hMod["queue"] == 49898190 &&
              hMod["mail"] == 49855051 && max == 1999 && maxLine == 1062 && err5 == 59808 && hourErr[13] == 2503 &&
              users.Count == 5000;
    Program.Report("log_processing", sw, ok);
  }
}

static class ValidacaoDadosBench {
  sealed class Rec {
    public string Cpf, Mail, Date, Amount;
  }

  static readonly MinStd random = new MinStd(8086);
  static long Rand(long n) => random.Next(n);

  static bool CpfOk(string c) {
    string d = c.Replace(".", "").Replace("-", "");
    if (d.Length != 11) return false;
    foreach (char ch in d)
      if (!char.IsAsciiDigit(ch)) return false;
    long s = 0;
    for (int i = 1; i <= 9; i++) s += (d[i - 1] - 48) * (11 - i);
    if (s * 10 % 11 % 10 != d[9] - 48) return false;
    s = 0;
    for (int i = 1; i <= 10; i++) s += (d[i - 1] - 48) * (12 - i);
    return s * 10 % 11 % 10 == d[10] - 48;
  }

  static bool EmailOk(string e) {
    int at = e.IndexOf('@') + 1;
    if (at < 2 || e.AsSpan(at).Contains('@')) return false;
    for (int i = 0; i < at - 1; i++)
      if (!"abcdefghijklmnopqrstuvwxyz0123456789._".Contains(e[i])) return false;
    string dom = e.Substring(at);
    if (dom.Length < 3 || !dom.Contains('.') || dom[0] == '.' || dom[^1] == '.') return false;
    foreach (char ch in dom)
      if (!"abcdefghijklmnopqrstuvwxyz0123456789.-".Contains(ch)) return false;
    return true;
  }

  static bool AmountOk(string c) {
    foreach (char ch in c)
      if (!char.IsAsciiDigit(ch)) return false;
    long n = long.Parse(c, CultureInfo.InvariantCulture);
    return n >= 1 && n <= 999999;
  }

  // CToD( c ) with SET DATE BRITISH and SET CENTURY ON: whether it is a date.
  static bool DateOk(string c) =>
    DateTime.TryParseExact(c, "dd/MM/yyyy", CultureInfo.InvariantCulture, DateTimeStyles.None, out _);

  // A record; its errors come from the seed as the checks' constants were computed.
  static Rec MakeRecord() {
    var d = new long[11];
    long s = 0;
    for (int k = 0; k < 9; k++) d[k] = Rand(10);
    for (int k = 1; k <= 9; k++) s += d[k - 1] * (11 - k);
    d[9] = s * 10 % 11 % 10;
    s = 0;
    for (int k = 1; k <= 10; k++) s += d[k - 1] * (12 - k);
    d[10] = s * 10 % 11 % 10;
    if (Rand(4) == 0) d[10] = (d[10] + 1) % 10;
    var cpf = new StringBuilder();
    for (int k = 1; k <= 11; k++) cpf.Append(d[k - 1]).Append(k == 3 || k == 6 ? "." : k == 9 ? "-" : "");
    var mail = new StringBuilder();
    long len = Rand(8) + 3;
    for (long k = 1; k <= len; k++) mail.Append("abcdefghijklmnopqrstuvwxyz0123456789"[(int)Rand(36)]);
    long kind = Rand(20);
    string dom = new[] { "mail.com", "example.com.br", "x.org" }[Rand(3)];
    if (kind == 0)
      mail.Append(dom);
    else if (kind == 1)
      mail.Append("@examplecom");
    else if (kind == 2)
      mail.Append("!@").Append(dom);
    else
      mail.Append('@').Append(dom);
    string day = (Rand(31) + 1).ToString("D2"), month = (Rand(12) + 1).ToString("D2");
    string date = day + "/" + month + "/" + (1990 + Rand(41));
    len = Rand(7) + 1;
    var amount = new StringBuilder();
    for (long k = 1; k <= len; k++) amount.Append(Rand(10));
    if (Rand(10) == 0) amount[(int)Rand(len)] = 'x'; // Stuff( cAmt, Rand( nLen ) + 1, 1, "x" )
    return new Rec { Cpf = cpf.ToString(), Mail = mail.ToString(), Date = date, Amount = amount.ToString() };
  }

  public static void Run() {
    var recs = new Rec[250000];
    for (int i = 0; i < recs.Length; i++) recs[i] = MakeRecord();
    var sw = Stopwatch.StartNew();
    long nC = 0, nE = 0, nD = 0, nA = 0, all = 0, sum = 0;
    foreach (Rec r in recs) {
      bool c = CpfOk(r.Cpf), e = EmailOk(r.Mail), d = DateOk(r.Date), a = AmountOk(r.Amount);
      if (a) sum += long.Parse(r.Amount, CultureInfo.InvariantCulture);
      nC += c ? 1 : 0;
      nE += e ? 1 : 0;
      nD += d ? 1 : 0;
      nA += a ? 1 : 0;
      all += c && e && d && a ? 1 : 0;
    }
    bool ok = nC == 187295 && nE == 212602 && nD == 245457 && nA == 192603 && all == 120445 && sum == 19490512431;
    Program.Report("validacao_dados", sw, ok);
  }
}

static class GeracaoHtmlBench {
  sealed class Item {
    public string Name;
    public long Qty, Price;
  }

  static readonly MinStd random = new MinStd(4711);
  static long Rand(long n) => random.Next(n);

  static string Escape(string c) =>
    c.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("\"", "&quot;");

  public static void Run() {
    string[] words = { "Alpha", "Beta & Sons", "<Gamma>", "Delta", "Echo \"Q\"", "Foxtrot", "Golf", "H&M" };
    var items = new Item[100000];
    for (int i = 0; i < items.Length; i++) {
      string first = words[Rand(8)];
      var it = new Item { Name = first + " " + words[Rand(8)] };
      it.Qty = Rand(100) + 1;
      it.Price = Rand(1000000) + 1;
      items[i] = it;
    }
    var sw = Stopwatch.StartNew();
    var c = new StringBuilder("<html><head><title>Items</title></head><body>\n<table>\n" +
                              "<tr><th>Id</th><th>Name</th><th>Qty</th><th>Price</th><th>Total</th></tr>\n");
    int pos77 = 0;
    for (int i = 1; i <= items.Length; i++) {
      Item r = items[i - 1];
      string id = i.ToString(CultureInfo.InvariantCulture);
      if (i == 77777) pos77 = c.Length + 1;
      string row = "<tr id=\"r" + id + "\" class=\"" + (i % 2 == 1 ? "odd" : "even") + "\"><td>" + id + "</td><td>" +
                   Escape(r.Name) + "</td><td class=\"n\">" + XbNum.Str(r.Qty, 10, 0).TrimStart() +
                   "</td><td class=\"n\">" + XbNum.Str(r.Price / 100.0, 12, 2).TrimStart() + "</td><td class=\"n\">" +
                   XbNum.Str(r.Qty * r.Price / 100.0, 14, 2).TrimStart() + "</td></tr>\n";
      c.Append(row);
    }
    c.Append("</table>\n</body></html>\n");
    string s = c.ToString();
    string row77 = "<tr id=\"r77777\" class=\"odd\"><td>77777</td><td>&lt;Gamma&gt; Delta</td><td class=\"n\">56</td>" +
                   "<td class=\"n\">3992.87</td><td class=\"n\">223600.72</td></tr>\n";
    string tail = "<tr id=\"r100000\" class=\"even\"><td>100000</td><td>Foxtrot Alpha</td><td class=\"n\">57</td>" +
                  "<td class=\"n\">4506.67</td><td class=\"n\">256880.19</td></tr>\n</table>\n</body></html>\n";
    bool ok = s.Length == 15121698 && pos77 == 11756338 && s.Substring(pos77 - 1, row77.Length) == row77 &&
              s.EndsWith(tail, StringComparison.Ordinal);
    Program.Report("geracao_html", sw, ok);
  }
}

static class IndexacaoSimuladaBench {
  static readonly MinStd random = new MinStd(99991);
  static long Rand(long n) => random.Next(n);

  // The first position (from 0) whose entry is not below k, compared over k's length as < does with SET EXACT OFF
  // (the count when there is none).
  static int LowerBound(List<string> a, string k) {
    int lo = 0, hi = a.Count;
    while (lo < hi) {
      int m = (lo + hi) / 2;
      if (string.CompareOrdinal(a[m], 0, k, 0, k.Length) < 0)
        lo = m + 1;
      else
        hi = m;
    }
    return lo;
  }

  public static void Run() {
    int recs = 150000;
    var arr = new string[recs];
    for (int i = 1; i <= recs; i++) arr[i - 1] = "C" + Rand(10000000).ToString("D8") + i.ToString("D6");
    var sw = Stopwatch.StartNew();
    Array.Sort(arr, StringComparer.Ordinal);
    var a = new List<string>(arr);
    long hits = 0, sum = 0, chk = 0;
    bool ok = true;
    for (int i = 1; i <= 100000; i++) {
      string k = "C" + Rand(10000000).ToString("D8");
      int p = LowerBound(a, k);
      if (p < a.Count && a[p].StartsWith(k, StringComparison.Ordinal)) {
        hits++;
        sum += long.Parse(a[p].AsSpan(a[p].Length - 6), CultureInfo.InvariantCulture);
      }
    }
    for (int i = 1; i <= 3000; i++) {
      string k = "C" + Rand(10000000).ToString("D8") + (recs + i).ToString("D6");
      a.Insert(LowerBound(a, k), k); // ASize and AIns
    }
    for (int i = 1; i < a.Count; i++) ok = ok && string.CompareOrdinal(a[i - 1], a[i]) < 0;
    for (int i = 0; i < a.Count; i += 997)
      chk = (chk * 31 + long.Parse(a[i].AsSpan(a[i].Length - 6), CultureInfo.InvariantCulture)) % 1000000007;
    ok = ok && hits == 1426 && sum == 107771969 && chk == 409559053 && a[0] == "C00000040027737" &&
         a[^1] == "C09999947069110";
    Program.Report("indexacao_simulada", sw, ok);
  }
}

// SET DATE BRITISH and SET CENTURY ON: DToC and CToD as "dd/MM/yyyy"; TString and Secs as "hh:mm:ss".
static class DateTimeOpsBench {
  static readonly MinStd random = new MinStd(1999);
  static long Rand(long n) => random.Next(n);

  static DateTime SToD(string s) => DateTime.ParseExact(s, "yyyyMMdd", CultureInfo.InvariantCulture);
  static string DToS(DateTime d) => d.ToString("yyyyMMdd", CultureInfo.InvariantCulture);

  // The date n months after y-m-d, its day made the month's last when the month is shorter.
  static DateTime AddMonths(int y, int m, int day, int n) {
    int nM = m + n;
    int nY = y + (nM - 1) / 12;
    nM = (nM - 1) % 12 + 1;
    DateTime first = SToD(nY.ToString("D4") + nM.ToString("D2") + "01");
    DateTime next = first.AddDays(31);
    return first.AddDays(Math.Min(day, next.AddDays(-next.Day).Day) - 1);
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var dBase = SToD("19000101");
    long ymd = 0, sun = 0, fri13 = 0, add = 0, toEnd = 0, secs = 0, bad = 0;
    for (int i = 1; i <= 200000; i++) {
      DateTime d = dBase.AddDays(Rand(73000));
      int y = d.Year, m = d.Month, day = d.Day;
      long n = y * 10000 + m * 100 + day;
      ymd += n;
      string dtoc = d.ToString("dd/MM/yyyy", CultureInfo.InvariantCulture);
      if (long.Parse(DToS(d), CultureInfo.InvariantCulture) != n ||
          DateTime.ParseExact(dtoc, "dd/MM/yyyy", CultureInfo.InvariantCulture) != d)
        bad++;
      if (d.DayOfWeek == DayOfWeek.Sunday) sun++; // DoW( d ) == 1
      if (day == 13 && d.ToString("dddd", CultureInfo.InvariantCulture) == "Friday") fri13++;
      add += long.Parse(DToS(AddMonths(y, m, day, 13)), CultureInfo.InvariantCulture);
      toEnd += (SToD(y.ToString().PadLeft(4) + "1231") - d).Days;
      long s = Rand(86400);
      string time = TimeSpan.FromSeconds(s).ToString(@"hh\:mm\:ss", CultureInfo.InvariantCulture);
      if ((long)TimeSpan.ParseExact(time, @"hh\:mm\:ss", CultureInfo.InvariantCulture).TotalSeconds != s) bad++;
      secs += s;
    }
    bool ok = bad == 0 && ymd == 3998853334023 && sun == 28567 && fri13 == 951 && add == 4001021793716 &&
              toEnd == 36531947 && secs == 8632784536;
    Program.Report("date_time_ops", sw, ok);
  }
}

static class NumericFormattingBench {
  static readonly MinStd random = new MinStd(314159);
  static long Rand(long n) => random.Next(n);

  static int Count(string c, char ch) => c.Length - c.Replace(ch.ToString(), "").Length;

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var c = new StringBuilder();
    long cents = 0, tenths = 0;
    for (int i = 1; i <= 80000; i++) {
      long xi = Rand(100000000) - 50000000;
      if (Math.Abs(xi) % 10 == 5) xi++;
      double x = xi / 1000.0;
      long n = Rand(1000000);
      string line = XbNum.Str(x, 14, 2) + XbNum.StrZero(n, 8) + XbNum.Transform(x, "@E 999,999.99") +
                    XbNum.Transform(x, "999,999.999") + XbNum.TransformR(XbNum.StrZero(n, 11), "@R 999.999.999-99") +
                    XbNum.Transform(x, "@( 99,999.99") + XbNum.Transform(n % 3, "@Z 9") +
                    XbNum.Str(XbNum.Round(x, 1), 12, 1).TrimStart() + "\n";
      c.Append(line);
      cents += (long)XbNum.Round(XbNum.Val(XbNum.Str(x, 14, 2)) * 100, 0);
      tenths += (long)XbNum.Round(XbNum.Round(x, 1) * 10, 0);
    }
    string s = c.ToString();
    long comma = Count(s, ','), dot = Count(s, '.'), paren = Count(s, '(');
    // The first line, the counts and the text as a whole were checked against the reference implementation; the sums
    // of cents and tenths against a C computation.
    bool ok = s.Length == 6065015 && comma == 236856 && dot == 558428 && paren == 41318 && cents == -6496080269 &&
              tenths == -649608444 &&
              s.StartsWith("      35103.0200005610 35.103,02 35,103.019000.000.056-1035,103.02 35103.0\n",
                           StringComparison.Ordinal);
    Program.Report("numeric_formatting", sw, ok);
  }
}
