// bench/upper_lower_alltrim.prg in JavaScript: toUpperCase(), toLowerCase(), trim(), trimStart() and trimEnd() of
// 1,000 mixed-case strings padded with blanks on both sides, 1,500,000 times.
const { report } = require("./bench.js");

let seed = 8;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

const chars = "aBcDeFgHiJkLmNoPqRsTuVwXyZ 0123456789";
const a = new Array(1000);
for (let i = 0; i < a.length; i++) {
  let w = "";
  // FOR ... TO evaluates its limit at every test, as here: Rand() is called on each.
  for (let j = 1; j <= 5 + rand(30); j++) w += chars.charAt(rand(37));
  a[i] = " ".repeat(rand(6)) + "x" + w + "Q" + " ".repeat(rand(6));
}
const t = performance.now();
let n = 0;
for (let i = 1; i <= 1500000; i++) {
  const x = a[i % 1000];
  const w = x.trim();
  const lower = w.toLowerCase();
  n += w.length + w.toUpperCase().charCodeAt(0) + lower.charCodeAt(lower.length - 1) + x.trimEnd().length -
    x.trimStart().length;
}
report("upper_lower_alltrim", t, n === 320344500);
