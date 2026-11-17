// bench/mixed.prg in JavaScript: integers and doubles in one expression, double to integer, comparisons across both.
const { report } = require("./bench.js");
const t = performance.now();
let s = 0, n = 0;
for (let i = 1; i <= 20000000; i++) {
  const d = i + 3.14;
  s += Math.trunc(d * 4) % 7;
  if (d > i + 3) n++;
  s += i / 4;
}
report("mixed", t, s === 50000062499998 && n === 20000000);
