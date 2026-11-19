// bench/hash_massivo.prg in JavaScript: 1,000,000 numeric keys drawn from a fixed-seed generator counted in one Map
// (about 632,000 distinct), then 1,500,000 lookups of keys drawn from twice the range, about two thirds missing.
const { report } = require("./bench.js");

let seed = 2024;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

const t = performance.now();
const h = new Map();
let hits = 0, sum = 0;
for (let i = 1; i <= 1000000; i++) {
  const k = rand(1000000);
  if (h.has(k)) h.set(k, h.get(k) + 1);
  else h.set(k, 1);
}
for (let i = 1; i <= 1500000; i++) {
  const k = rand(2000000);
  if (h.has(k)) {
    hits++;
    sum += h.get(k);
  }
}
report("hash_massivo", t, h.size === 631967 && hits === 473472 && sum === 748930);
