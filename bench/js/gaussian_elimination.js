// bench/gaussian_elimination.prg in JavaScript: doubles in nested arrays, a 300 x 300 system of random doubles (a
// fixed seed) whose solution is all ones, solved by Gaussian elimination with partial pivoting (rows swapped as
// references) and back substitution, 6 times. seed * 1103515245 passes 2^53; Math.imul keeps the low 32 bits of the
// product exact, and only the low 31 are kept. The arrays keep the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");
const t = performance.now();
const n = 300;
let ok = true;
for (let rep = 1; rep <= 6; rep++) {
  let seed = 42;
  const a = [null];
  for (let i = 1; i <= n; i++) a.push(new Array(n + 2).fill(null));
  for (let i = 1; i <= n; i++) {
    let s = 0.0;
    for (let j = 1; j <= n; j++) {
      seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
      a[i][j] = seed / 2147483648 - 0.5;
      s = s + a[i][j];
    }
    a[i][n + 1] = s;
  }
  for (let k = 1; k <= n; k++) {
    let p = k;
    for (let i = k + 1; i <= n; i++) if (Math.abs(a[i][k]) > Math.abs(a[p][k])) p = i;
    const piv = a[p];
    a[p] = a[k];
    a[k] = piv;
    for (let i = k + 1; i <= n; i++) {
      const row = a[i];
      const f = row[k] / piv[k];
      for (let j = k; j <= n + 1; j++) row[j] = row[j] - f * piv[j];
    }
  }
  const x = new Array(n + 1).fill(null);
  let err = 0.0;
  for (let i = n; i >= 1; i--) {
    const row = a[i];
    let s = row[n + 1];
    for (let j = i + 1; j <= n; j++) s = s - row[j] * x[j];
    x[i] = s / row[i];
    err = Math.max(err, Math.abs(x[i] - 1.0));
  }
  ok = ok && err < 0.000000001;
}
report("gaussian_elimination", t, ok);
