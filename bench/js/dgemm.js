// bench/dgemm.prg in JavaScript: C := alpha A B + beta C on 250 x 250 matrices of doubles in flat row-major arrays
// (loops in i, k, j order), the inputs from a linear congruential generator, 3 times; the check is the sum and the
// trace of C. seed * 1103515245 passes 2^53; Math.imul keeps the low 32 bits of the product exact, and only the low
// 31 are kept. The arrays keep the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");
const t = performance.now();
const n = 250, alpha = 1.5, beta = 0.5;
const a = new Array(n * n + 1), b = new Array(n * n + 1), c = new Array(n * n + 1);
let seed = 7;
for (let i = 1; i <= n * n; i++) {
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
  a[i] = seed / 1073741824.0 - 1.0;
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
  b[i] = seed / 1073741824.0 - 1.0;
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
  c[i] = seed / 1073741824.0 - 1.0;
}
for (let rep = 1; rep <= 3; rep++) {
  for (let i = 0; i <= n - 1; i++) {
    const ci = i * n;
    for (let j = ci + 1; j <= ci + n; j++) c[j] = beta * c[j];
    for (let k = 0; k <= n - 1; k++) {
      const aik = alpha * a[ci + k + 1];
      const bk = k * n - ci;
      for (let j = ci + 1; j <= ci + n; j++) c[j] += aik * b[bk + j];
    }
  }
}
let sum = 0.0, trace = 0.0;
for (let i = 1; i <= n; i++) {
  for (let j = 1; j <= n; j++) sum += c[(i - 1) * n + j];
  trace += c[(i - 1) * n + i];
}
report("dgemm", t, Math.abs(sum + 145.62810535074) < 0.000001 && Math.abs(trace - 342.80906789597) < 0.000001);
