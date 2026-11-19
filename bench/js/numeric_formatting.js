// bench/numeric_formatting.prg in JavaScript: 80,000 numbers with three decimals (-50,000 to 50,000) and integers
// (0 to 999,999) from a fixed seed, each formatted with toFixed() and padStart() (width and decimals, leading zeros),
// Intl.NumberFormat (thousands separators; the separators exchanged as @E; a negative number in parentheses as @(;
// a blank for zero as @Z), a CPF mask inserted into a string as @R, rounded to one decimal by Round()'s formula, and
// read back with Number(). The text's length, its separators and the values read back are checked.
const { report } = require("./bench.js");

let seed = 314159;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

// Str( v, nWidth, nDec ): a minus sign only when a non-zero digit is shown, stars when the number does not fit.
function str(v, width, dec) {
  let s = v.toFixed(dec);
  if (s[0] === "-" && !/[1-9]/.test(s)) s = s.substring(1);
  return s.length > width ? "*".repeat(width) : s.padStart(width);
}
const zero = (n, width) => String(n).padStart(width, "0");
// Round( v, nDec ), the xBase formula: half away from zero, the half added to ten times the scaled value, the sum
// divided back and raised by one unit in the last place before truncating (so 2.675 rounds up). The product by
// Number.EPSILON, a power of two, is exact: the sum rounds once, as the runtime's fma.
function round(v, dec) {
  const p = Math.pow(10, dec);
  if (v === 0) return 0;
  if (Math.abs(v) * p >= 2 ** 52) return v;
  let tenfold = v * p * 10;
  tenfold += tenfold < 0 ? -5 : 5;
  const back = tenfold / 10;
  return Math.trunc(back + back * Number.EPSILON) / p;
}
const grouped2 = new Intl.NumberFormat("en-US", { minimumFractionDigits: 2, maximumFractionDigits: 2,
  signDisplay: "negative" });
const grouped3 = new Intl.NumberFormat("en-US", { minimumFractionDigits: 3, maximumFractionDigits: 3,
  signDisplay: "negative" });
const exchange = (s) => s.replace(/[.,]/g, (c) => (c === "." ? "," : "."));
// "@( 99,999.99": the magnitude in 9 columns, a negative number with "(" over the first column and ")" after; when
// a digit is in that column there is no room and the digits become stars.
function parens(v) {
  const s = grouped2.format(Math.abs(v)).padStart(9);
  if (v >= 0) return s;
  return "(" + (s[0] === " " ? s.substring(1) : s.substring(1).replace(/[0-9]/g, "*")) + ")";
}
// "@R 999.999.999-99" on an 11-digit string: the separators inserted.
const cpf = (s) => s.substring(0, 3) + "." + s.substring(3, 6) + "." + s.substring(6, 9) + "-" + s.substring(9, 11);

const t = performance.now();
let c = "", cents = 0, tenths = 0;
for (let i = 1; i <= 80000; i++) {
  let x = rand(100000000) - 50000000;
  if (Math.abs(x) % 10 === 5) x++;
  x /= 1000;
  const n = rand(1000000);
  const line = str(x, 14, 2) + zero(n, 8) + exchange(grouped2.format(x)).padStart(10) +
    grouped3.format(x).padStart(11) + cpf(zero(n, 11)) + parens(x) + (n % 3 === 0 ? " " : String(n % 3)) +
    str(round(x, 1), 12, 1).trimStart() + "\n";
  c += line;
  cents += round(Number(str(x, 14, 2)) * 100, 0);
  tenths += round(round(x, 1) * 10, 0);
}
const commas = c.length - c.replaceAll(",", "").length;
const dots = c.length - c.replaceAll(".", "").length;
const parenCount = c.length - c.replaceAll("(", "").length;
report("numeric_formatting", t, c.length === 6065015 && commas === 236856 && dots === 558428 &&
  parenCount === 41318 && cents === -6496080269 && tenths === -649608444 &&
  c.substring(0, 75) === "      35103.0200005610 35.103,02 35,103.019000.000.056-1035,103.02 35103.0\n");
