// bench/hash_resize.prg in JavaScript: 20 rounds of 100,000 keys added to an empty Map and deleted again, the even
// ones first, then the odd ones.
const { report } = require("./bench.js");

const t = performance.now();
let sum = 0, ok = true;
for (let r = 1; r <= 20; r++) {
  const h = new Map();
  for (let i = 1; i <= 100000; i++) h.set(i * 7 + r, i);
  ok = ok && h.size === 100000;
  for (let i = 2; i <= 100000; i += 2) {
    sum += h.get(i * 7 + r);
    h.delete(i * 7 + r);
  }
  ok = ok && h.size === 50000;
  for (let i = 1; i <= 100000; i += 2) {
    sum += h.get(i * 7 + r);
    h.delete(i * 7 + r);
  }
  ok = ok && h.size === 0;
}
report("hash_resize", t, ok && sum === 100001000000);
