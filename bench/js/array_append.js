// bench/array_append.prg in JavaScript: 5 arrays of 2,000,000 numbers grown with push() from empty, then summed.
const { report } = require("./bench.js");

const t = performance.now();
let a, sum = 0;
for (let r = 1; r <= 5; r++) {
  a = [];
  for (let i = 1; i <= 2000000; i++) a.push(i + r);
  for (const x of a) sum += x;
}
report("array_append", t, sum === 10000035000000 && a.length === 2000000);
