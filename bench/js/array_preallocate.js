// bench/array_preallocate.prg in JavaScript: 25 arrays of 2,000,000 numbers made at their size with new Array( n ),
// filled by index, then summed by index.
const { report } = require("./bench.js");

const t = performance.now();
const n = 2000000;
let a, sum = 0;
for (let r = 1; r <= 25; r++) {
  a = new Array(n);
  for (let i = 0; i < n; i++) a[i] = i + 1 + r;
  for (let i = 0; i < n; i++) sum += a[i];
}
report("array_preallocate", t, sum === 50000675000000 && a.length === n);
