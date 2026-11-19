// xbase's Str(), StrZero(), Val(), Round() and Transform() rules, for the ports whose text or sums depend on them
// (runtime/src/core/numfmt.c and runtime/src/rtl/transform.c; docs/runtime.md, Display): Transform() only with the
// functions and template characters the benchmarks use. And the generator most .prg files define as Rand().
using System;
using System.Globalization;
using System.Text;

static class XbNum {
  const double DblEpsilon = 2.220446049250313e-16; // C's DBL_EPSILON (double.Epsilon is the least subnormal)
  const double DblIntegral = 4503599627370496.0; // 2^52: the doubles from there up are a rounding step apart or more

  // Str( x, width, dec ): x rounded to dec decimals, right-justified in width characters, stars when it does not fit;
  // a minus sign only when a non-zero digit is shown. The digits are .NET's, rounded from the double's exact value:
  // xbase's own rule (16 significant digits, a 4 followed by 9s counting as a 5) gives the same for these numbers.
  public static string Str(double x, int width, int dec) {
    string digits = Math.Abs(x).ToString("F" + dec, CultureInfo.InvariantCulture);
    bool negative = x < 0 && digits.AsSpan().IndexOfAnyExcept("0.") >= 0;
    string s = negative ? "-" + digits : digits;
    return dec >= width || s.Length > width ? new string('*', width) : s.PadLeft(width);
  }

  // Str( x ) of a number with SET DECIMALS (2) decimals, as every remainder (%) is: 10 integer columns.
  public static string Str2(double x) => Str(x, 13, 2);

  public static string StrZero(double x, int width, int dec = 0) {
    string s = Str(x, width, dec);
    int blanks = s.Length - s.TrimStart(' ').Length;
    if (blanks == 0) return s;
    return s[blanks] == '-' ? "-" + new string('0', blanks) + s.Substring(blanks + 1)
                            : new string('0', blanks) + s.Substring(blanks);
  }

  // Val( c ): the number at the start of c (after blanks: a sign, digits, a point and digits), 0 without one.
  public static double Val(string c) {
    int start = 0;
    while (start < c.Length && c[start] == ' ') start++;
    int end = start;
    if (end < c.Length && c[end] == '-') end++;
    while (end < c.Length && (char.IsAsciiDigit(c[end]) || c[end] == '.')) end++;
    return double.TryParse(c.AsSpan(start, end - start), NumberStyles.AllowLeadingSign | NumberStyles.AllowDecimalPoint,
                           CultureInfo.InvariantCulture, out double v) ? v : 0;
  }

  // Round( v, d ), half away from zero as Clipper: the half added to ten times the scaled value, the sum divided back
  // and raised by about one unit in the last place before truncating; a scaled value from 2^52 up is its own.
  public static double Round(double v, int d) {
    double p = Math.Pow(10.0, Math.Abs(d));
    if (v == 0.0 || (d < 0 && double.IsInfinity(p))) return 0.0;
    if (d >= 0 && Math.Abs(v) * p >= DblIntegral) return v;
    double tenfold = (d >= 0 ? v * p : v / p) * 10.0;
    tenfold += tenfold < 0 ? -5.0 : 5.0;
    double back = tenfold / 10.0;
    double r = Math.Truncate(Math.FusedMultiplyAdd(back, DblEpsilon, back));
    return d >= 0 ? r / p : r * p;
  }

  // Transform( x, cPicture ) of a number: the functions @E (`,` and `.` exchanged), @( (a negative number in
  // parentheses) and @Z (blanks for a zero), and a template of 9, `,` and `.`.
  public static string Transform(double x, string picture) {
    ParseFunctions(picture, out bool british, out bool parens, out bool blank, out string pic);
    int width = 0, dec = 0;
    for (int i = 0; i < pic.Length; i++) {
      if (pic[i] == '.') {
        while (++i < pic.Length)
          if (pic[i] == '9') {
            width++;
            dec++;
          }
        if (dec > 0) width++;
        break;
      }
      if (pic[i] == '9') width++;
    }
    string str = Str(parens && x < 0 ? -x : x, width, dec);
    var o = new StringBuilder();
    int used = 0;
    for (int i = 0; i < pic.Length; i++) {
      char p = pic[i];
      if (p == '9')
        o.Append(used < width ? str[used++] : ' ');
      else if (p == '.' && used < width) {
        o.Append(british ? ',' : '.');
        used++;
      } else if (p == ',' && i > 0 && used < width) {
        char before = o[i - 1];
        if (char.IsAsciiDigit(before))
          o.Append(british ? '.' : ',');
        else { // before the digits: what precedes it moves right
          o.Append(before);
          if (before == '-') o[i - 1] = i > 1 && o[i - 2] != '$' ? o[i - 2] : ' ';
        }
      } else
        o.Append(p);
    }
    if (x < 0 && parens) {
      // no room for the parenthesis: the digits become stars
      if (o[0] >= '1' && o[0] <= '9' && (pic[0] == '9' || pic[0] != o[0]))
        for (int k = 1; k < o.Length; k++)
          if (char.IsAsciiDigit(o[k]) && (pic[k] == '9' || pic[k] != o[k])) o[k] = '*';
      o[0] = '(';
      o.Append(')');
    }
    return blank && x == 0 ? new string(' ', o.Length) : o.ToString();
  }

  // Transform( c, "@R template" ): the string's characters on the template's placeholders (copied as they are: no
  // template here has ! or Y), the other template characters inserted.
  public static string TransformR(string c, string picture) {
    ParseFunctions(picture, out _, out _, out _, out string pic);
    var o = new StringBuilder();
    int at = 0;
    foreach (char p in pic) {
      if (at >= c.Length) {
        o.Append("!#9aAlLnNxXyY".Contains(p) ? ' ' : p);
        continue;
      }
      if ("!#9aAlLnNxXyY".Contains(p))
        o.Append(c[at++]);
      else
        o.Append(p);
    }
    return o.ToString();
  }

  static void ParseFunctions(string picture, out bool british, out bool parens, out bool blank, out string template) {
    british = parens = blank = false;
    template = picture;
    if (picture.Length == 0 || picture[0] != '@') return;
    int i = 1;
    for (; i < picture.Length && picture[i] != ' '; i++)
      switch (char.ToUpperInvariant(picture[i])) {
      case 'E': british = true; break;
      case '(': parens = true; break;
      case 'Z': blank = true; break;
      }
    template = i < picture.Length ? picture.Substring(i + 1) : "";
  }
}

// The .prg's Rand( n ): the minimal standard generator (Park and Miller), seed * 16807 % (2^31 - 1), its remainder
// by n.
sealed class MinStd {
  long seed;
  public MinStd(long seed) { this.seed = seed; }
  public long Next(long n) {
    seed = seed * 16807 % 2147483647;
    return seed % n;
  }
}
