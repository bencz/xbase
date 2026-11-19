// bench/monte_carlo_pi.prg in JavaScript: pi by the Monte Carlo method, 50,000,000 points of the unit square from a
// linear congruential generator (31 bits), the count of those inside the quarter circle. seed * 1103515245 passes
// 2^53; Math.imul keeps the low 32 bits of the product exact, and only the low 31 are kept.
const { report } = require("./bench.js");
const t = performance.now();
const n = 50000000;
let seed = 42, inside = 0;
for (let i = 1; i <= n; i++) {
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
  const x = seed / 2147483648.0;
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
  const y = seed / 2147483648.0;
  if (x * x + y * y <= 1.0) inside++;
}
const est = (4.0 * inside) / n;
report("monte_carlo_pi", t, inside === 39267523 && Math.abs(est - 3.14159) < 0.01);
