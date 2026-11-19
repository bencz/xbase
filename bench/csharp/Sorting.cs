// bench/counting_sort, merge_sort, heap_sort, radix_sort, crc_adler_fnv and md5_sha1 .prg in C#: the same work and
// checks. Arrays keep the .prg's 1-based indices (element 0 unused); the buffers hashed are strings, built through a
// StringBuilder.
using System;
using System.Diagnostics;
using System.Text;

static class CountingSortBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 1000000, m = 65536;
    var a = new int[n + 1];
    var b = new int[n + 1];
    var cnt = new int[m + 1];
    bool ok = true;
    long sum = 0;
    for (int rep = 1; rep <= 6; rep++) {
      long seed = rep + 20;
      for (int i = 1; i <= n; i++) {
        seed = seed * 16807 % 2147483647;
        a[i] = (int)(seed / 32768);
      }
      Array.Fill(cnt, 0);
      for (int i = 1; i <= n; i++) cnt[a[i] + 1]++;
      int pos = 1;
      for (int k = 1; k <= m; k++) {
        int c = cnt[k];
        cnt[k] = pos;
        pos += c;
      }
      for (int i = 1; i <= n; i++) {
        int k = a[i] + 1;
        b[cnt[k]] = a[i];
        cnt[k]++;
      }
      for (int i = 2; i <= n; i++)
        if (b[i - 1] > b[i]) ok = false;
      for (int i = 1; i <= n; i++) sum += (long)b[i] * (i % 8 + 1);
    }
    Program.Report("counting_sort", sw, ok && sum == 884633960242);
  }
}

static class MergeSortBench {
  // Sorts a[ lo .. hi ]; w is the work array the halves merge through.
  static void MergeSort(long[] a, long[] w, int lo, int hi) {
    if (hi - lo < 1) return;
    int mid = (lo + hi) / 2;
    MergeSort(a, w, lo, mid);
    MergeSort(a, w, mid + 1, hi);
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
      if (a[j] < a[i])
        w[k] = a[j++];
      else
        w[k] = a[i++];
      k++;
    }
    while (i <= mid) w[k++] = a[i++];
    while (j <= hi) w[k++] = a[j++];
    for (k = lo; k <= hi; k++) a[k] = w[k];
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 300000;
    var a = new long[n + 1];
    var w = new long[n + 1];
    bool ok = true;
    long sum = 0;
    for (int rep = 1; rep <= 3; rep++) {
      long seed = rep;
      for (int i = 1; i <= n; i++) {
        seed = seed * 16807 % 2147483647;
        a[i] = seed;
      }
      MergeSort(a, w, 1, n);
      for (int i = 2; i <= n; i++)
        if (a[i - 1] > a[i]) ok = false;
      for (int i = 1; i <= n; i++) sum += a[i] * (i % 8 + 1);
    }
    Program.Report("merge_sort", sw, ok && sum == 4349496350073402);
  }
}

static class HeapSortBench {
  // Moves a[ i ] down the heap a[ 1 .. n ] until neither child is larger.
  static void SiftDown(long[] a, int i, int n) {
    long x = a[i];
    int c;
    while ((c = i * 2) <= n) {
      if (c < n && a[c + 1] > a[c]) c++;
      if (a[c] <= x) break;
      a[i] = a[c];
      i = c;
    }
    a[i] = x;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 300000;
    var a = new long[n + 1];
    bool ok = true;
    long sum = 0;
    for (int rep = 1; rep <= 3; rep++) {
      long seed = rep + 10;
      for (int i = 1; i <= n; i++) {
        seed = seed * 16807 % 2147483647;
        a[i] = seed;
      }
      for (int i = n / 2; i >= 1; i--) SiftDown(a, i, n);
      for (int i = n; i >= 2; i--) {
        (a[1], a[i]) = (a[i], a[1]);
        SiftDown(a, 1, i - 1);
      }
      for (int i = 2; i <= n; i++)
        if (a[i - 1] > a[i]) ok = false;
      for (int i = 1; i <= n; i++) sum += a[i] * (i % 8 + 1);
    }
    Program.Report("heap_sort", sw, ok && sum == 4351668891394344);
  }
}

static class RadixSortBench {
  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 300000;
    var a = new long[n + 1];
    var b = new long[n + 1];
    var cnt = new int[257];
    bool ok = true;
    long sum = 0;
    for (int rep = 1; rep <= 10; rep++) {
      long seed = rep + 30;
      for (int i = 1; i <= n; i++) {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        a[i] = seed;
      }
      for (int d = 0; d <= 3; d++) {
        int shift = d * 8;
        Array.Fill(cnt, 0);
        for (int i = 1; i <= n; i++) cnt[((a[i] >> shift) & 0xFF) + 1]++;
        int pos = 1;
        for (int k = 1; k <= 256; k++) {
          int x = cnt[k];
          cnt[k] = pos;
          pos += x;
        }
        for (int i = 1; i <= n; i++) {
          long x = a[i];
          long k = ((x >> shift) & 0xFF) + 1;
          b[cnt[k]] = x;
          cnt[k]++;
        }
        (a, b) = (b, a);
      }
      for (int i = 2; i <= n; i++)
        if (a[i - 1] > a[i]) ok = false;
      for (int i = 1; i <= n; i++) sum += a[i] * ((i & 7) + 1);
    }
    Program.Report("radix_sort", sw, ok && sum == 14501540638159929);
  }
}

static class CrcAdlerFnvBench {
  static long Crc32Of(long[] table, string s) {
    long crc = 0xFFFFFFFF;
    for (int i = 0; i < s.Length; i++) crc = table[(crc ^ s[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
  }

  static long Adler32Of(string s) {
    long a = 1, b = 0;
    for (int i = 0; i < s.Length; i++) {
      a += s[i];
      if (a >= 65521) a -= 65521;
      b += a;
      if (b >= 65521) b -= 65521;
    }
    return (b << 16) | a;
  }

  static long Fnv1aOf(string s) {
    long h = 2166136261;
    for (int i = 0; i < s.Length; i++) h = ((h ^ s[i]) * 16777619) & 0xFFFFFFFF;
    return h;
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 1000000;
    long seed = 42, total = 0;
    var table = new long[256];
    for (long i = 0; i <= 255; i++) {
      long c = i;
      for (int k = 1; k <= 8; k++) c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
      table[i] = c;
    }
    bool ok = Crc32Of(table, "123456789") == 0xCBF43926 && Adler32Of("Wikipedia") == 0x11E60398 &&
              Fnv1aOf("a") == 0xE40C292C;
    for (int rep = 1; rep <= 20; rep++) {
      var data = new StringBuilder();
      for (int i = 1; i <= n; i++) {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        data.Append((char)((seed >> 16) & 0xFF));
      }
      string s = data.ToString();
      total += Crc32Of(table, s) + Adler32Of(s) + Fnv1aOf(s);
    }
    Program.Report("crc_adler_fnv", sw, ok && total == 142519858956);
  }
}

// The 32-bit words are uint, whose sums wrap as the .prg's masks make them.
static class Md5Sha1Bench {
  static readonly uint[] K = {
    0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE, 0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
    0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE, 0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
    0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA, 0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
    0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED, 0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
    0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C, 0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
    0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05, 0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
    0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039, 0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
    0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1, 0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391 };
  static readonly int[] R = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };

  static uint RotL(uint x, int s) => (x << s) | (x >> (32 - s));

  // The message, a 0x80 byte, zeros up to 56 bytes modulo 64, then its length in bits in 8 bytes, the least
  // significant first when little.
  static string Pad(string s, bool little) {
    int n = s.Length, len = (n + 8) / 64 * 64 + 64;
    string bits = "";
    for (int i = 0; i <= 7; i++) {
      string b = ((char)((n * 8L >> (8 * i)) & 0xFF)).ToString();
      bits = little ? bits + b : b + bits;
    }
    return s + (char)0x80 + new string('\0', len - n - 9) + bits;
  }

  // The words as lowercase hexadecimal, each word's bytes the least significant first when little.
  static string Hex(uint[] words, bool little) {
    const string digits = "0123456789abcdef";
    var sb = new StringBuilder();
    foreach (uint w in words)
      for (int b = 0; b <= 3; b++) {
        uint by = (w >> (little ? 8 * b : 24 - 8 * b)) & 0xFF;
        sb.Append(digits[(int)(by >> 4)]);
        sb.Append(digits[(int)(by & 15)]);
      }
    return sb.ToString();
  }

  static string Md5Of(string s) {
    string p = Pad(s, true);
    uint[] h = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };
    var m = new uint[16];
    for (int blk = 0; blk < p.Length; blk += 64) {
      for (int j = 0; j <= 15; j++) {
        int q = blk + 4 * j;
        m[j] = p[q] | ((uint)p[q + 1] << 8) | ((uint)p[q + 2] << 16) | ((uint)p[q + 3] << 24);
      }
      uint a = h[0], b = h[1], c = h[2], d = h[3];
      for (int i = 0; i <= 63; i++) {
        uint f;
        int g;
        if (i < 16) {
          f = (b & c) | (~b & d);
          g = i;
        } else if (i < 32) {
          f = (d & b) | (~d & c);
          g = (5 * i + 1) & 15;
        } else if (i < 48) {
          f = b ^ c ^ d;
          g = (3 * i + 5) & 15;
        } else {
          f = c ^ (b | ~d);
          g = (7 * i) & 15;
        }
        uint tmp = d;
        d = c;
        c = b;
        b = b + RotL(a + f + K[i] + m[g], R[(i >> 4) * 4 + (i & 3)]);
        a = tmp;
      }
      h[0] += a;
      h[1] += b;
      h[2] += c;
      h[3] += d;
    }
    return Hex(h, true);
  }

  static string Sha1Of(string s) {
    string p = Pad(s, false);
    uint[] h = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    var w = new uint[80];
    for (int blk = 0; blk < p.Length; blk += 64) {
      for (int j = 0; j <= 15; j++) {
        int q = blk + 4 * j;
        w[j] = ((uint)p[q] << 24) | ((uint)p[q + 1] << 16) | ((uint)p[q + 2] << 8) | p[q + 3];
      }
      for (int j = 16; j < 80; j++) w[j] = RotL(w[j - 3] ^ w[j - 8] ^ w[j - 14] ^ w[j - 16], 1);
      uint a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
      for (int i = 0; i <= 79; i++) {
        uint f, k;
        if (i < 20) {
          f = (b & c) | (~b & d);
          k = 0x5A827999;
        } else if (i < 40) {
          f = b ^ c ^ d;
          k = 0x6ED9EBA1;
        } else if (i < 60) {
          f = (b & c) | (b & d) | (c & d);
          k = 0x8F1BBCDC;
        } else {
          f = b ^ c ^ d;
          k = 0xCA62C1D6;
        }
        uint tmp = RotL(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = RotL(b, 30);
        b = a;
        a = tmp;
      }
      h[0] += a;
      h[1] += b;
      h[2] += c;
      h[3] += d;
      h[4] += e;
    }
    return Hex(h, false);
  }

  public static void Run() {
    var sw = Stopwatch.StartNew();
    int n = 200000;
    long seed = 7;
    bool ok = Md5Of("") == "d41d8cd98f00b204e9800998ecf8427e" && Md5Of("abc") == "900150983cd24fb0d6963f7d28e17f72" &&
              Sha1Of("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709" &&
              Sha1Of("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d";
    var sb = new StringBuilder();
    for (int i = 1; i <= n; i++) {
      seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
      sb.Append((char)((seed >> 16) & 0xFF));
    }
    string data = sb.ToString();
    for (int rep = 1; rep <= 40; rep++)
      ok = ok && Md5Of(data) == "9caadd7dbbe1174da261d335b92f85fb" &&
           Sha1Of(data) == "cec3d79455d923d234a1a40741be135a4ba980e2";
    Program.Report("md5_sha1", sw, ok);
  }
}
