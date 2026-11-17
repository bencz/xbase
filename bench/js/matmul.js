// bench/matmul.prg in JavaScript: the product of two 100 x 100 integer matrices, 20 times.
const { report } = require("./bench.js");
const t = performance.now();
const n = 100;
const mk = () => Array.from({ length: n }, () => new Array(n).fill(0));
const a = mk(), b = mk(), c = mk();
for (let i = 0; i < n; i++)
  for (let j = 0; j < n; j++) {
    a[i][j] = (i + j) % 7;
    b[i][j] = (i * j) % 5;
  }
let total = 0;
for (let rep = 1; rep <= 20; rep++)
  for (let i = 0; i < n; i++)
    for (let j = 0; j < n; j++) {
      let s = 0;
      for (let k = 0; k < n; k++) s += a[i][k] * b[k][j];
      c[i][j] = s;
      total += s;
    }
report("matmul", t, total === 20 * 4798200);
