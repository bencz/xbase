// bench/fasta, k_nucleotide, reverse_complement, levenshtein, base64, csv_parse, at_rat_strtran, jaro_winkler,
// longest_common_subsequence, string_builder, string_concatenation, substr_massivo, tokenize_split,
// upper_lower_alltrim, url_encode, word_count, ini_parse, xml_simples and long_short_mix .prg in C#: the same work
// and checks, a FOR limit with a call in it evaluated at every test as xbase does. A string the .prg grows with += is
// a StringBuilder, its bytes chars 0 to 255; At(), RAt() and StrTran() are IndexOf, LastIndexOf and Replace
// (ordinal), its hashes Dictionary.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;

// The fasta benchmark's sequences, shared by fasta and reverse_complement.
static class FastaText {
  const int IM = 139968, IA = 3877, IC = 29573, Line = 60;
  const string Alu = "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTC" +
                     "AGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCG" +
                     "TGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGG" +
                     "AGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";
  public const string IubSymbols = "acgtBDHKMNRSVWY", SapiensSymbols = "acgt";
  public static readonly double[] IubProb = {
    0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02 };
  public static readonly double[] SapiensProb = { 0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008 };

  // n characters of the ALU sequence, cyclically, in lines of 60.
  public static string RepeatSeq(int n) {
    var sb = new StringBuilder();
    int pos = 0;
    for (int done = 0; done < n; done += Line) {
      int m = Math.Min(n - done, Line);
      for (int i = 1; i <= m; i++) {
        sb.Append(Alu[pos]);
        if (++pos >= Alu.Length) pos = 0;
      }
      sb.Append('\n');
    }
    return sb.ToString();
  }

  // n symbols drawn with the LCG in state[ 0 ] (the sum of their indexes, from 0, added to state[ 1 ]), in lines of
  // 60.
  public static string RandomSeq(long[] state, string sym, double[] prob, int n) {
    var cum = new double[prob.Length];
    double acc = 0;
    for (int i = 0; i < prob.Length; i++) {
      acc += prob[i];
      cum[i] = acc;
    }
    long seed = state[0], picks = 0;
    var sb = new StringBuilder();
    for (int done = 0; done < n; done += Line) {
      int m = Math.Min(n - done, Line);
      for (int i = 1; i <= m; i++) {
        seed = (seed * IA + IC) % IM;
        double r = (double)seed / IM;
        int j = 0;
        while (j < prob.Length - 1 && r >= cum[j]) j++;
        picks += j;
        sb.Append(sym[j]);
      }
      sb.Append('\n');
    }
    state[0] = seed;
    state[1] += picks;
    return sb.ToString();
  }
}

static class FastaBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 1500000;
    long[] state = { 42, 0 };
    var output = new StringBuilder();
    output.Append(">ONE Homo sapiens alu\n");
    output.Append(FastaText.RepeatSeq(2 * n));
    output.Append(">TWO IUB ambiguity codes\n");
    output.Append(FastaText.RandomSeq(state, FastaText.IubSymbols, FastaText.IubProb, 3 * n));
    output.Append(">THREE Homo sapiens frequency\n");
    output.Append(FastaText.RandomSeq(state, FastaText.SapiensSymbols, FastaText.SapiensProb, 5 * n));
    string s = output.ToString();
    int two = s.IndexOf(">TWO", StringComparison.Ordinal) + 1;
    bool ok = s.Length == 15250077 && state[0] == 28650 && state[1] == 25406718 && two == 3050023 &&
              s.Substring(two - 22, 21) == "TGGGCGACAGAGCGAGACTC\n" && s.EndsWith("acacatatagttttatacaa\n",
                                                                                 StringComparison.Ordinal);
    Program.Report("fasta", sw, ok);
  }
}

static class KNucleotideBench {
  const int IM = 139968, IA = 3877, IC = 29573;

  // Every k-mer of s with its count.
  static Dictionary<string, long> Frames(string s, int k) {
    var h = new Dictionary<string, long>();
    for (int i = 0; i <= s.Length - k; i++) {
      string key = s.Substring(i, k);
      h[key] = h.TryGetValue(key, out long c) ? c + 1 : 1;
    }
    return h;
  }

  static long CountOf(string s, string seq) => Frames(s, seq.Length).TryGetValue(seq, out long c) ? c : 0;

  // The k-mers by count, then by name: "A 151655 T 150452 ...".
  static string Sorted(string s, int k) {
    var pairs = new List<KeyValuePair<string, long>>(Frames(s, k));
    pairs.Sort((x, y) => x.Value != y.Value ? y.Value.CompareTo(x.Value) : string.CompareOrdinal(x.Key, y.Key));
    var sb = new StringBuilder();
    foreach (var p in pairs) {
      if (sb.Length > 0) sb.Append(' ');
      sb.Append(p.Key).Append(' ').Append(p.Value);
    }
    return sb.ToString();
  }

  public static void Run() {
    int n = 100000;
    long seed = 42;
    var cum = new double[4];
    double acc = 0;
    for (int i = 0; i < 4; i++) {
      acc += FastaText.SapiensProb[i];
      cum[i] = acc;
    }
    for (int i = 1; i <= 3 * n; i++) seed = (seed * IA + IC) % IM;
    var sb = new StringBuilder();
    for (int i = 1; i <= 5 * n; i++) {
      seed = (seed * IA + IC) % IM;
      double r = (double)seed / IM;
      int j = 0;
      while (j < 3 && r >= cum[j]) j++;
      sb.Append("ACGT"[j]);
    }
    string s = sb.ToString();
    var sw = Stopwatch.StartNew();
    bool ok = Sorted(s, 1) == "A 151655 T 150452 C 99156 G 98737" &&
              Sorted(s, 2) == "AA 45992 TA 45599 AT 45574 TT 45299 CA 30129 AC 30096 AG 29993 GA 29935 CT 29857 " +
                              "TC 29832 GT 29722 TG 29721 GC 19617 CC 19611 CG 19559 GG 19463" &&
              CountOf(s, "GGT") == 5871 && CountOf(s, "GGTA") == 1769 && CountOf(s, "GGTATT") == 185 &&
              CountOf(s, "GGTATTTTAATT") == 4 && CountOf(s, "GGTATTTTAATTTATAGT") == 4;
    Program.Report("k_nucleotide", sw, ok);
  }
}

static class ReverseComplementBench {
  const int Line = 60;

  // The sequence reverse-complemented, in lines of 60.
  static string RevComp(string seq, char[] comp) {
    var sb = new StringBuilder();
    int col = 0;
    for (int i = seq.Length - 1; i >= 0; i--) {
      sb.Append(comp[seq[i]]);
      if (++col == Line) {
        sb.Append('\n');
        col = 0;
      }
    }
    if (col > 0) sb.Append('\n');
    return sb.ToString();
  }

  public static void Run() {
    int n = 2500000;
    long[] state = { 42, 0 };
    string input = ">ONE Homo sapiens alu\n" + FastaText.RepeatSeq(2 * n) + ">TWO IUB ambiguity codes\n" +
                   FastaText.RandomSeq(state, FastaText.IubSymbols, FastaText.IubProb, 3 * n) +
                   ">THREE Homo sapiens frequency\n" +
                   FastaText.RandomSeq(state, FastaText.SapiensSymbols, FastaText.SapiensProb, 5 * n);
    var sw = Stopwatch.StartNew();
    var comp = new char[256];
    const string from = "ACBDGHKMNSRUTWVYacbdghkmnsrutwvy", to = "TGVHCDMKNSYAABWRTGVHCDMKNSYAABWR";
    for (int k = 0; k < 256; k++) comp[k] = (char)k;
    for (int k = 0; k < from.Length; k++) comp[from[k]] = to[k];
    var output = new StringBuilder();
    var seq = new StringBuilder();
    int i = 0;
    while (i < input.Length) {
      char c = input[i];
      if (c == '>') {
        if (seq.Length > 0) {
          output.Append(RevComp(seq.ToString(), comp));
          seq.Clear();
        }
        while ((c = input[i++]) != '\n') output.Append(c);
        output.Append(c);
      } else {
        if (c != '\n') seq.Append(c);
        i++;
      }
    }
    output.Append(RevComp(seq.ToString(), comp));
    string s = output.ToString();
    int two = s.IndexOf(">TWO", StringComparison.Ordinal) + 1;
    int three = s.IndexOf(">THREE", StringComparison.Ordinal) + 1;
    bool ok = s.Length == 25416745 &&
              s.Substring(22, 60) == "GTAGCTGGGATTACAGGCGCGCGCCACCACGCCCGGCTAATTTTTGTATTTTTAGTAGAG" &&
              s.Substring(two + 24, 60) == "TTTATTYSTTGTATCCCTMWCGAACTAVGYHTATANTAWCARAWTTTVCTSTKDNTATHA" &&
              s.Substring(three + 29, 60) == "GTAATTTCTCTCGGCTATTAGATTTGAACATATACACAGGCCAGATGATCTGGAAAAATC" &&
              s.EndsWith("CTATAACTAATATATTCGTC\n", StringComparison.Ordinal);
    Program.Report("reverse_complement", sw, ok);
  }
}

static class LevenshteinBench {
  static readonly MinStd random = new MinStd(1001);
  static int Rand(int n) => (int)random.Next(n);

  static int Distance(string s, string u) {
    int m = s.Length, n = u.Length;
    var prev = new int[n + 1];
    var cur = new int[n + 1];
    for (int j = 0; j <= n; j++) prev[j] = j;
    for (int i = 1; i <= m; i++) {
      cur[0] = i;
      char c = s[i - 1];
      for (int j = 1; j <= n; j++)
        cur[j] = Math.Min(Math.Min(prev[j], cur[j - 1]) + 1, prev[j - 1] + (c == u[j - 1] ? 0 : 1));
      (prev, cur) = (cur, prev);
    }
    return prev[n];
  }

  static string[] Pair(int len) {
    var s = new StringBuilder();
    var u = new StringBuilder();
    for (int i = 0; i < len; i++) s.Append((char)(97 + Rand(4)));
    for (int i = 0; i < len; i++) {
      int k = Rand(10);
      if (k == 0)
        u.Append((char)(97 + Rand(4)));
      else if (k == 1)
        u.Append(s[i]).Append((char)(97 + Rand(4)));
      else if (k != 2)
        u.Append(s[i]);
    }
    return new[] { s.ToString(), u.ToString() };
  }

  public static void Run() {
    var pairs = new string[10000][];
    for (int i = 0; i < pairs.Length; i++) pairs[i] = Pair(30 + Rand(31));
    var sw = Stopwatch.StartNew();
    long n = 0;
    foreach (var p in pairs) n += Distance(p[0], p[1]);
    Program.Report("levenshtein", sw, n == 108309 && Distance("kitten", "sitting") == 3);
  }
}

// Base64 written out, as the .prg writes it (not Convert.ToBase64String).
static class Base64Bench {
  const string Table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static readonly int[] Value = MakeValues();
  static readonly MinStd random = new MinStd(64);

  static int Rand(int n) => (int)random.Next(n);

  static int[] MakeValues() {
    var v = new int[256];
    for (int i = 0; i < 64; i++) v[Table[i]] = i;
    return v;
  }

  static string Encode(string c) {
    var sb = new StringBuilder();
    int len = c.Length, rest = len % 3, n;
    for (int i = 0; i < len - rest; i += 3) {
      n = c[i] * 65536 + c[i + 1] * 256 + c[i + 2];
      sb.Append(Table[n / 262144]).Append(Table[n / 4096 % 64]).Append(Table[n / 64 % 64]).Append(Table[n % 64]);
    }
    if (rest == 1) {
      n = c[len - 1];
      sb.Append(Table[n / 4]).Append(Table[n % 4 * 16]).Append("==");
    } else if (rest == 2) {
      n = c[len - 2] * 256 + c[len - 1];
      sb.Append(Table[n / 1024]).Append(Table[n / 16 % 64]).Append(Table[n % 16 * 4]).Append('=');
    }
    return sb.ToString();
  }

  static string Decode(string c) {
    var sb = new StringBuilder();
    int pad = c.EndsWith("==", StringComparison.Ordinal) ? 2 : c.EndsWith('=') ? 1 : 0;
    for (int i = 0; i < c.Length; i += 4) {
      int n = Value[c[i]] * 262144 + Value[c[i + 1]] * 4096 + Value[c[i + 2]] * 64 + Value[c[i + 3]];
      sb.Append((char)(n / 65536)).Append((char)(n / 256 % 256)).Append((char)(n % 256));
    }
    sb.Length -= pad;
    return sb.ToString();
  }

  public static void Run() {
    var data = new StringBuilder();
    for (int i = 1; i <= 1200000; i++) data.Append((char)Rand(256));
    string c = data.ToString();
    var sw = Stopwatch.StartNew();
    long n = 0;
    bool ok = true;
    for (int r = 1; r <= 3; r++) {
      string part = c.Substring(0, c.Length - r + 1);
      string enc = Encode(part);
      ok = ok && Decode(enc) == part;
      n += enc.Length;
      for (int i = 0; i < enc.Length; i += 97) n += enc[i];
    }
    ok = ok && n == 9038319 && Encode("Ma") == "TWE=" && Decode("TQ==") == "M";
    Program.Report("base64", sw, ok);
  }
}

static class CsvParseBench {
  static readonly MinStd random = new MinStd(1999);
  static int Rand(int n) => (int)random.Next(n);

  static List<List<string>> ParseCsv(string c) {
    var recs = new List<List<string>>();
    var rec = new List<string>();
    var field = new StringBuilder();
    bool quoted = false;
    for (int i = 0; i < c.Length; i++) {
      char x = c[i];
      if (quoted) {
        if (x == '"') {
          if (i + 1 < c.Length && c[i + 1] == '"') {
            field.Append(x);
            i++;
          } else
            quoted = false;
        } else
          field.Append(x);
      } else if (x == '"')
        quoted = true;
      else if (x == ',') {
        rec.Add(field.ToString());
        field.Clear();
      } else if (x == '\n') {
        rec.Add(field.ToString());
        recs.Add(rec);
        rec = new List<string>();
        field.Clear();
      } else
        field.Append(x);
    }
    return recs;
  }

  // A field: plain letters, or quoted with a comma, a doubled quote or a line break inside.
  static string MakeField() {
    int k = Rand(6);
    char ch = (char)(97 + Rand(26));
    string w = new string(ch, 3 + Rand(12));
    if (k == 0) return "\"" + w + ", " + w + "\"";
    if (k == 1) return "\"say \"\"" + w + "\"\"\"";
    if (k == 2) return "\"" + w + "\n" + w + "\"";
    return w;
  }

  public static void Run() {
    var sb = new StringBuilder();
    for (int i = 1; i <= 40000; i++) {
      string line = i + "," + MakeField() + "," + XbNum.Str2(Rand(100000)).TrimStart() + ",";
      line += MakeField() + "," + new string('w', Rand(10)) + "\n";
      sb.Append(line);
    }
    string c = sb.ToString();
    var sw = Stopwatch.StartNew();
    long n = 0, nQuoted = 0;
    List<List<string>> recs = null;
    for (int r = 1; r <= 6; r++) {
      recs = ParseCsv(c);
      foreach (var a in recs) {
        n += (long)(XbNum.Val(a[0]) + XbNum.Val(a[2])) + a[1].Length + a[3].Length + a.Count;
        if (a[3].Contains('\n') || a[1].Contains('"')) nQuoted++;
      }
    }
    Program.Report("csv_parse", sw, recs.Count == 40000 && n == 16750579326 && nQuoted == 73038);
  }
}

static class AtRatStrTranBench {
  static readonly MinStd random = new MinStd(33);
  static int Rand(int n) => (int)random.Next(n);

  public static void Run() {
    var words = new string[200];
    for (int i = 0; i < words.Length; i++) {
      var w = new StringBuilder();
      for (int j = 1; j <= 3 + Rand(6); j++) w.Append((char)(97 + Rand(26)));
      words[i] = w.ToString();
    }
    var sb = new StringBuilder();
    while (sb.Length < 20000) sb.Append(words[Rand(Rand(200) + 1)]).Append(' ');
    string c = sb.ToString();
    var sw = Stopwatch.StartNew();
    long n = 0;
    for (int r = 1; r <= 80; r++)
      foreach (string w in words)
        n += c.IndexOf(w, StringComparison.Ordinal) + 1 + c.LastIndexOf(w, StringComparison.Ordinal) + 1 +
             c.Replace(w, "<" + w + ">").Length;
    Program.Report("at_rat_strtran", sw, n == 620527280);
  }
}

static class JaroWinklerBench {
  static readonly MinStd random = new MinStd(271);
  static int Rand(int n) => (int)random.Next(n);

  static double JaroWinkler(string s1, string s2) {
    int l1 = s1.Length, l2 = s2.Length, m = 0, trans = 0, prefix = 0;
    if (l1 == 0 || l2 == 0) return l1 == l2 ? 1 : 0;
    int range = Math.Max(Math.Max(l1, l2) / 2 - 1, 0);
    var a1 = new bool[l1];
    var a2 = new bool[l2];
    for (int i = 0; i < l1; i++)
      for (int j = Math.Max(0, i - range); j <= Math.Min(l2 - 1, i + range); j++)
        if (!a2[j] && s1[i] == s2[j]) {
          a1[i] = true;
          a2[j] = true;
          m++;
          break;
        }
    if (m == 0) return 0;
    int k = 0;
    for (int i = 0; i < l1; i++)
      if (a1[i]) {
        while (!a2[k]) k++;
        if (s1[i] != s2[k]) trans++;
        k++;
      }
    double jaro = ((double)m / l1 + (double)m / l2 + (m - trans / 2.0) / m) / 3;
    while (prefix < 4 && prefix < Math.Min(l1, l2) && s1[prefix] == s2[prefix]) prefix++;
    return jaro + prefix * 0.1 * (1 - jaro);
  }

  static string[] Pair(int len) {
    var s = new StringBuilder();
    for (int i = 0; i < len; i++) s.Append((char)(65 + Rand(6)));
    string u = s.ToString();
    for (int i = 1; i <= 3; i++) {
      int k = Rand(u.Length) + 1, del = Rand(2);
      char ins = (char)(65 + Rand(6));
      u = u.Substring(0, k - 1) + ins + u.Substring(Math.Min(u.Length, k - 1 + del)); // Stuff( u, k, del, ins )
    }
    return new[] { s.ToString(), u };
  }

  public static void Run() {
    var pairs = new string[300000][];
    for (int i = 0; i < pairs.Length; i++) pairs[i] = Pair(6 + Rand(9));
    var sw = Stopwatch.StartNew();
    double sum = 0;
    foreach (var p in pairs) sum += JaroWinkler(p[0], p[1]);
    bool ok = Math.Abs(sum - 260943.0356007) < 0.000001 &&
              Math.Abs(JaroWinkler("MARTHA", "MARHTA") - 0.961111111) < 0.000000001;
    Program.Report("jaro_winkler", sw, ok);
  }
}

static class LongestCommonSubsequenceBench {
  static readonly MinStd random = new MinStd(31);
  static int Rand(int n) => (int)random.Next(n);

  static int Lcs(string s, string u) {
    char[] aS = s.ToCharArray(), aU = u.ToCharArray();
    int m = aS.Length, n = aU.Length;
    var prev = new int[n + 1];
    var cur = new int[n + 1];
    for (int i = 0; i < m; i++) {
      char c = aS[i];
      for (int j = 1; j <= n; j++) cur[j] = c == aU[j - 1] ? prev[j - 1] + 1 : Math.Max(prev[j], cur[j - 1]);
      (prev, cur) = (cur, prev);
    }
    return prev[n];
  }

  static string[] Pair(int len) {
    var s = new StringBuilder();
    var u = new StringBuilder();
    for (int i = 0; i < len; i++) s.Append((char)(65 + Rand(4)));
    for (int i = 0; i < len; i++) {
      if (Rand(4) != 0) u.Append(s[i]);
      if (Rand(5) == 0) u.Append((char)(65 + Rand(4)));
    }
    return new[] { s.ToString(), u.ToString() };
  }

  public static void Run() {
    var pairs = new string[600][];
    for (int i = 0; i < pairs.Length; i++) pairs[i] = Pair(200 + Rand(101));
    var sw = Stopwatch.StartNew();
    long n = 0;
    foreach (var p in pairs) n += Lcs(p[0], p[1]);
    Program.Report("longest_common_subsequence", sw, n == 118188 && Lcs("ABCBDAB", "BDCABA") == 4);
  }
}

static class StringBuilderBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    string[] names = { "ant", "bee", "cat", "dog", "eel", "fox", "gnu", "hen", "ibis", "jay" };
    long n = 0;
    for (int r = 1; r <= 3; r++) {
      var sb = new StringBuilder();
      for (int i = 1; i <= 400000; i++)
        sb.Append("<tr><td>" + names[i % 10] + "</td><td>" + (i + r) + "</td></tr>" + "\n");
      string c = sb.ToString();
      n += c.Length;
      for (int i = 0; i < c.Length; i += 1000) n += c[i];
    }
    Program.Report("string_builder", sw, n == 47588053);
  }
}

static class StringConcatenationBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    string[] a = { "alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta" };
    string[] b = { "red", "green", "blue", "cyan", "magenta" };
    long n = 0;
    for (int i = 1; i <= 3000000; i++) {
      string s = a[i % 7] + "-" + b[i % 5] + "/" + a[i % 3] + ":" + b[i % 4];
      n += s.Length + s[^1];
    }
    Program.Report("string_concatenation", sw, n == 378692857);
  }
}

static class SubstrMassivoBench {
  static readonly MinStd random = new MinStd(5);
  static int Rand(int n) => (int)random.Next(n);

  public static void Run() {
    var sb = new StringBuilder();
    for (int i = 1; i <= 1000000; i++) sb.Append((char)(33 + Rand(90)));
    string c = sb.ToString();
    var sw = Stopwatch.StartNew();
    long n = 0;
    for (long i = 1; i <= 6000000; i++) {
      string s = c.Substring((int)(i * 7919 % 999970), (int)(i % 20 + 1));
      n += s[0] + s[^1] + s.Length;
    }
    Program.Report("substr_massivo", sw, n == 994529563);
  }
}

static class TokenizeSplitBench {
  static readonly MinStd random = new MinStd(64);
  static int Rand(int n) => (int)random.Next(n);

  // Cut with IndexOf and Substring, as the .prg cuts with At() and SubStr().
  static List<string> Split(string c, char sep) {
    var a = new List<string>();
    int n;
    while ((n = c.IndexOf(sep)) >= 0) {
      a.Add(c.Substring(0, n));
      c = c.Substring(n + 1);
    }
    a.Add(c);
    return a;
  }

  public static void Run() {
    var lines = new string[200000];
    for (int i = 0; i < lines.Length; i++) {
      string first = XbNum.Str2(Rand(100000)).TrimStart() + ",name" + (i + 1) + ",";
      string third = XbNum.Str2(Rand(1000)).TrimStart() + ",,";
      lines[i] = first + third + new string('z', Rand(12)) + ",end";
    }
    var sw = Stopwatch.StartNew();
    long n = 0;
    for (int r = 1; r <= 2; r++)
      foreach (string c in lines) {
        var a = Split(c, ',');
        n += (long)(XbNum.Val(a[0]) + XbNum.Val(a[2])) + a.Count + a[4].Length;
      }
    Program.Report("tokenize_split", sw, n == 20190509958);
  }
}

static class UpperLowerAllTrimBench {
  static readonly MinStd random = new MinStd(8);
  static int Rand(int n) => (int)random.Next(n);

  public static void Run() {
    var a = new string[1000];
    const string chars = "aBcDeFgHiJkLmNoPqRsTuVwXyZ 0123456789";
    for (int i = 0; i < a.Length; i++) {
      var w = new StringBuilder();
      for (int j = 1; j <= 5 + Rand(30); j++) w.Append(chars[Rand(37)]);
      string left = new string(' ', Rand(6));
      a[i] = left + "x" + w + "Q" + new string(' ', Rand(6));
    }
    var sw = Stopwatch.StartNew();
    long n = 0;
    for (int i = 1; i <= 1500000; i++) {
      string x = a[i % 1000];
      string w = x.Trim(' ');
      n += w.Length + w.ToUpperInvariant()[0] + w.ToLowerInvariant()[^1] + x.TrimEnd(' ').Length -
           x.TrimStart(' ').Length;
    }
    Program.Report("upper_lower_alltrim", sw, n == 320344500);
  }
}

// Percent-encoding written out, as the .prg writes it.
static class UrlEncodeBench {
  const string HexDigits = "0123456789ABCDEF";
  const string Unreserved = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~";
  static readonly MinStd random = new MinStd(80);

  static int Rand(int n) => (int)random.Next(n);

  static string UrlEncode(string c) {
    var sb = new StringBuilder();
    foreach (char x in c)
      if (Unreserved.Contains(x))
        sb.Append(x);
      else if (x == ' ')
        sb.Append('+');
      else
        sb.Append('%').Append(HexDigits[x / 16]).Append(HexDigits[x % 16]);
    return sb.ToString();
  }

  static string UrlDecode(string c) {
    var sb = new StringBuilder();
    int i = 0;
    while (i < c.Length) {
      char x = c[i];
      if (x == '%') {
        sb.Append((char)(HexDigits.IndexOf(c[i + 1]) * 16 + HexDigits.IndexOf(c[i + 2])));
        i += 3;
      } else {
        sb.Append(x == '+' ? ' ' : x);
        i++;
      }
    }
    return sb.ToString();
  }

  public static void Run() {
    var a = new string[20000];
    const string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJ0123456789     -_.~&=/?%:#+éü";
    for (int i = 0; i < a.Length; i++) {
      var c = new StringBuilder();
      for (int j = 1; j <= 20 + Rand(61); j++) c.Append(chars[Rand(chars.Length)]);
      a[i] = c.ToString();
    }
    var sw = Stopwatch.StartNew();
    long n = 0;
    bool ok = true;
    for (int r = 1; r <= 8; r++)
      foreach (string c in a) {
        string enc = UrlEncode(c);
        n += enc.Length + enc[^1];
        ok = ok && UrlDecode(enc) == c;
      }
    ok = ok && n == 18744664 && UrlEncode("a b&c=é") == "a+b%26c%3D%E9";
    Program.Report("url_encode", sw, ok);
  }
}

static class WordCountBench {
  static readonly MinStd random = new MinStd(4242);
  static int Rand(int n) => (int)random.Next(n);

  public static void Run() {
    var vocab = new string[5000];
    var lines = new string[200000];
    for (int i = 0; i < vocab.Length; i++) {
      var w = new StringBuilder();
      for (int j = 1; j <= 2 + Rand(9); j++) w.Append("eEtaAoinNsrhldcumfpgwybvkxjqz"[Rand(Rand(29) + 1)]);
      vocab[i] = w.ToString();
    }
    for (int i = 0; i < lines.Length; i++) {
      var c = new StringBuilder();
      for (int j = 1; j <= 1 + Rand(30); j++) c.Append(vocab[Rand(Rand(5000) + 1)]).Append(' ');
      lines[i] = c.ToString();
    }
    var sw = Stopwatch.StartNew();
    var h = new Dictionary<string, long>();
    long words = 0, top = 0, check = 0;
    string topWord = "";
    foreach (string line in lines) {
      string c = line;
      int n;
      while ((n = c.IndexOf(' ')) >= 0) {
        string w = c.Substring(0, n).ToLowerInvariant();
        c = c.Substring(n + 1);
        words++;
        h[w] = h.TryGetValue(w, out long k) ? k + 1 : 1;
      }
    }
    foreach (long k in h.Values) {
      check += k * k;
      if (k > top) top = k;
    }
    foreach (string v in vocab)
      if (h.TryGetValue(v.ToLowerInvariant(), out long k) && k == top) {
        topWord = v.ToLowerInvariant();
        break;
      }
    bool ok = words == 1312183 && h.Count == 4328 && top == 7336 && topWord == "ee" && check == 971695061;
    Program.Report("word_count", sw, ok);
  }
}

static class IniParseBench {
  static readonly MinStd random = new MinStd(7777);
  static int Rand(int n) => (int)random.Next(n);

  // The sections of the text, each a dictionary of its keys; quoted gets the count of quoted values.
  static Dictionary<string, Dictionary<string, string>> ParseIni(string c, out int quoted) {
    var ini = new Dictionary<string, Dictionary<string, string>>();
    Dictionary<string, string> section = null;
    quoted = 0;
    foreach (string raw in c.Split('\n')) {
      string line = raw.Trim(' ');
      if (line.Length == 0 || line[0] == ';' || line[0] == '#') continue;
      if (line[0] == '[') {
        section = new Dictionary<string, string>();
        ini[line.Substring(1, line.Length - 2).ToLowerInvariant()] = section;
      } else {
        int eq = line.IndexOf('=');
        if (eq >= 0) {
          string key = line.Substring(0, eq).TrimEnd(' ').ToLowerInvariant();
          string val = line.Substring(eq + 1).TrimStart(' ');
          if (val.Length >= 2 && val[0] == '"' && val[^1] == '"') {
            val = val.Substring(1, val.Length - 2);
            quoted++;
          }
          section[key] = val;
        }
      }
    }
    return ini;
  }

  public static void Run() {
    var lines = new List<string>();
    for (int s = 1; s <= 400; s++) {
      lines.Add("; section " + s);
      lines.Add("[" + (Rand(2) == 1 ? "Section" : "SECTION") + s + "]");
      for (int k = 1; k <= 25; k++) {
        int sp = Rand(3);
        string n = Rand(100000).ToString();
        string val = k % 5 == 0 ? n : k % 5 == 1 ? "\"text " + n + "\"" : "w" + n;
        lines.Add((sp == 2 ? "  " : "") + "Key" + k.ToString("D2") + (sp > 0 ? " = " : "=") + val);
        if (k % 7 == 0) lines.Add("# comment");
      }
      lines.Add("");
    }
    var sb = new StringBuilder();
    foreach (string x in lines) sb.Append(x).Append('\n');
    string c = sb.ToString();
    var sw = Stopwatch.StartNew();
    bool ok = true;
    for (int i = 1; i <= 80; i++) {
      var h = ParseIni(c, out int quoted);
      long sum = 0;
      foreach (var x in h.Values)
        for (int k = 5; k <= 25; k += 5) sum += long.Parse(x["key" + k.ToString("D2")]);
      ok = ok && h.Count == 400 && sum == 102092980 && quoted == 2000 && h["section77"]["key11"] == "text 72557" &&
           h["section400"].Count == 25;
    }
    ok = ok && c.Length == 181694;
    Program.Report("ini_parse", sw, ok);
  }
}

static class XmlSimplesBench {
  static readonly MinStd random = new MinStd(5150);
  static int Rand(int n) => (int)random.Next(n);

  // The text between <tag> and </tag> in c.
  static string Inner(string c, string tag) {
    int start = c.IndexOf("<" + tag + ">", StringComparison.Ordinal) + tag.Length + 2;
    return c.Substring(start, c.IndexOf("</" + tag + ">", StringComparison.Ordinal) - start);
  }

  static string Escape(string c) =>
    c.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace("'", "&apos;");

  static string Unescape(string c) =>
    c.Replace("&apos;", "'").Replace("&gt;", ">").Replace("&lt;", "<").Replace("&amp;", "&");

  sealed class Book {
    public string Title, Author;
    public long Price, Year;
  }

  public static void Run() {
    string[] wordList = { "Rivers", "Salt & Pepper", "<Night>", "Stone", "Tales", "Tom & Jerry", "Glass", "Winter",
                          "Red", "Old" };
    string[] authors = { "Ana Lima", "Bruno Costa", "Carla Dias", "Diego Reis", "Eva O'Neil" };
    var books = new Book[50000];
    for (int i = 0; i < books.Length; i++) {
      int n = Rand(2) + 2;
      var title = new StringBuilder();
      for (int k = 1; k <= n; k++) title.Append(k > 1 ? " " : "").Append(wordList[Rand(10)]);
      var b = new Book { Title = title.ToString() };
      b.Author = authors[Rand(5)];
      b.Price = Rand(10000) + 100;
      b.Year = 1950 + Rand(75);
      books[i] = b;
    }
    var sw = Stopwatch.StartNew();
    var sb = new StringBuilder("<?xml version=\"1.0\"?>\n<catalog>\n");
    for (int i = 0; i < books.Length; i++) {
      Book b = books[i];
      string price = XbNum.Str(b.Price / 100.0, 10, 2).TrimStart();
      string book = "<book id=\"" + (i + 1) + "\"><title>" + Escape(b.Title) + "</title><author>" + Escape(b.Author) +
                    "</author><price>" + price + "</price><year>" + b.Year.ToString().PadLeft(4) + "</year></book>\n";
      sb.Append(book);
    }
    sb.Append("</catalog>\n");
    string c = sb.ToString();
    bool ok = c.Length == 6276273;
    string[] parts = c.Split("<book ");
    long ids = 0, cents = 0, amp = 0, years = 0, eva = 0;
    string title12345 = null;
    for (int i = 1; i < parts.Length; i++) {
      string book = parts[i];
      ids += (long)XbNum.Val(book.Substring(4));
      string title = Unescape(Inner(book, "title"));
      if (title.Contains('&')) amp++;
      if (i == 12345) title12345 = title;
      if (Unescape(Inner(book, "author")) == "Eva O'Neil") eva++;
      cents += (long)XbNum.Round(XbNum.Val(Inner(book, "price")) * 100, 0);
      years += (long)XbNum.Val(Inner(book, "year"));
    }
    ok = ok && parts.Length == 50001 && ids == 50000L * 50001 / 2 && cents == 255248286 && amp == 21225 &&
         years == 99351507 && eva == 10152 && title12345 == "Winter Glass Tom & Jerry";
    Program.Report("xml_simples", sw, ok);
  }
}

// The records are objects of a number, a string and a reference to another, as the .prg's arrays of three.
static class LongShortMixBench {
  sealed class Rec {
    public long N;
    public string S;
    public Rec Next;
  }

  static readonly MinStd random = new MinStd(17);
  static int Rand(int n) => (int)random.Next(n);

  public static void Run() {
    var sw = Stopwatch.StartNew();
    var aLong = new Rec[100000];
    long n = 0;
    for (int i = 0; i < aLong.Length; i++) aLong[i] = new Rec { N = i + 1, S = (i + 1).ToString().PadLeft(10) };
    for (int i = 1; i <= 2500000; i++) {
      var x = new Rec { N = i % 1000, S = new string((char)(97 + i % 26), i % 40 + 1) };
      n += x.S.Length;
      if (i % 8 == 0) aLong[Rand(aLong.Length)].Next = x;
      if (i % 64 == 0) {
        int k = Rand(aLong.Length);
        aLong[k] = new Rec { N = aLong[k].N + 1, S = i.ToString().PadLeft(10), Next = aLong[k].Next };
      }
    }
    foreach (Rec r in aLong) {
      n += r.N + r.S.Length;
      if (r.Next != null) n += r.Next.N;
    }
    Program.Report("long_short_mix", sw, n == 5099739142);
  }
}
