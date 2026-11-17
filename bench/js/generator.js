// bench/generator.prg in JavaScript: a generator of 3,000,000 numbers and a second one that yields the even ones of the
// first.
const { report } = require("./bench.js");
function* range(a, b) {
  for (let i = a; i <= b; i++) yield i;
}
function* evens(e) {
  for (const x of e) if (x % 2 === 0) yield x;
}
const t = performance.now();
let s = 0;
for (const x of evens(range(1, 3000000))) s += x;
report("generator", t, s === 2250001500000);
