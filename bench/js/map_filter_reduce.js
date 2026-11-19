// bench/map_filter_reduce.prg in JavaScript: map, filter and reduce written by hand (as the .prg writes them) over
// arrays with closures: an array of 1,000,000 numbers mapped, the result filtered and reduced to a sum; 8 rounds with
// other closures.
const { report } = require("./bench.js");

function map(a, b) {
  const out = new Array(a.length);
  for (let i = 0; i < a.length; i++) out[i] = b(a[i]);
  return out;
}
function filter(a, b) {
  const out = [];
  for (const x of a) if (b(x)) out.push(x);
  return out;
}
function reduce(a, b, acc) {
  for (const x of a) acc = b(acc, x);
  return acc;
}

const a = new Array(1000000);
for (let i = 0; i < a.length; i++) a[i] = (i + 1) % 1000;
const t = performance.now();
let n = 0;
for (let r = 1; r <= 8; r++) {
  n += reduce(filter(map(a, (x) => x * r + 1), (x) => x % 3 === 0), (acc, x) => acc + x, 0);
}
report("map_filter_reduce", t, n === 4492503000);
