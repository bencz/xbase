// bench/higher_order_chain.prg in JavaScript: chains of lazy iterator steps (the iterator helpers, and generators for
// TakeWhile and Distinct, which they lack) over an array of 500,000 numbers, each step fed by the one before: Where,
// Select, Skip, TakeWhile and Sum; Select with the position, Where, Take and Count; Distinct with a key and Count;
// 5 rounds.
const { report } = require("./bench.js");

function* takeWhile(it, test) {
  let n = 0;
  for (const x of it) {
    if (!test(x, ++n)) return;
    yield x;
  }
}
function* distinct(it, key) {
  const seen = new Set();
  for (const x of it) {
    const k = key(x);
    if (seen.has(k)) continue;
    seen.add(k);
    yield x;
  }
}
const sum = (it) => it.reduce((s, x) => s + x, 0);
function count(it) {
  let n = 0;
  for (const x of it) n++;
  return n;
}

const a = new Array(500000);
for (let i = 0; i < a.length; i++) a[i] = ((i + 1) * 37) % 1000;
const t = performance.now();
let n = 0;
for (let r = 1; r <= 5; r++) {
  n += sum(takeWhile(a.values().filter((x) => x % 3 !== 0).map((x) => x * r).drop(10), (x) => x < 4900));
  n += count(a.values().map((x, i) => x + (i + 1) % 7).filter((x) => x % 2 === 0).take(50000 + r));
  n += count(distinct(a.values(), (x) => x % (100 * r)));
}
report("higher_order_chain", t, n === 1663633050);
