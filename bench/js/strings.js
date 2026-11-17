// bench/strings.prg in JavaScript: Left, Upper, Len and At on 20,000,000 short strings.
const { report } = require("./bench.js");
const t = performance.now();
const src = "abcdefghij";
let total = 0;
for (let i = 1; i <= 20000000; i++) {
  const w = src.substring(0, (i % 10) + 1).toUpperCase();
  total += w.length + w.indexOf("D") + 1;
}
report("strings", t, total === 166000000);
