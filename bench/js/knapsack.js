// bench/knapsack.prg in JavaScript: 0/1 knapsack by dynamic programming over one array of capacities, 200 items
// (weights 1 .. 1000 and values 1 .. 1000 from a linear congruential generator) into capacities up to 10,000, 10 times
// with different seeds; the best values are checked. The arrays keep the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");
const t = performance.now();
const n = 200, cap = 10000, best = new Array(cap + 2), w = new Array(n + 1), v = new Array(n + 1);
let total = 0;
for (let rep = 1; rep <= 10; rep++) {
  let seed = rep;
  for (let i = 1; i <= n; i++) {
    seed = (seed * 16807) % 2147483647;
    w[i] = (Math.floor(seed / 65536) % 1000) + 1;
    seed = (seed * 16807) % 2147483647;
    v[i] = (Math.floor(seed / 65536) % 1000) + 1;
  }
  best.fill(0);
  for (let i = 1; i <= n; i++) {
    const wi = w[i], vi = v[i];
    for (let c = cap + 1; c >= wi + 1; c--) {
      const x = best[c - wi] + vi;
      if (x > best[c]) best[c] = x;
    }
  }
  total += best[cap + 1];
}
report("knapsack", t, total === 366516);
