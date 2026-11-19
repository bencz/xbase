// bench/long_short_mix.prg in JavaScript: 100,000 records that live to the end, 2,500,000 temporary arrays and
// strings, one in 8 of them stored into a long-lived record (an old value pointing to a new one), one record in 64
// steps replaced by a new one.
const { report } = require("./bench.js");

let seed = 17;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}
// Str( n ) of an integer: right-aligned in 10 columns.
const str = (n) => String(n).padStart(10);

const t = performance.now();
const long = new Array(100000);
let n = 0;
for (let i = 0; i < long.length; i++) long[i] = [i + 1, str(i + 1), null];
for (let i = 1; i <= 2500000; i++) {
  const x = [i % 1000, String.fromCharCode(97 + (i % 26)).repeat((i % 40) + 1), null];
  n += x[1].length;
  if (i % 8 === 0) long[rand(long.length)][2] = x;
  if (i % 64 === 0) {
    const k = rand(long.length);
    long[k] = [long[k][0] + 1, str(i), long[k][2]];
  }
}
for (const r of long) {
  n += r[0] + r[1].length;
  if (r[2] !== null) n += r[2][0];
}
report("long_short_mix", t, n === 5099739142);
