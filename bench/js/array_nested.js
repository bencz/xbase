// bench/array_nested.prg in JavaScript: a 100 x 100 x 100 cube of nested arrays, filled through a[ i ][ j ][ k ], then
// 10 passes that change the cells from their neighbors along the last two dimensions and sum them.
const { report } = require("./bench.js");

const t = performance.now();
const a = new Array(100);
for (let i = 0; i < 100; i++) {
  a[i] = new Array(100);
  for (let j = 0; j < 100; j++) a[i][j] = new Array(100);
}
for (let i = 1; i <= 100; i++) {
  for (let j = 1; j <= 100; j++) {
    for (let k = 1; k <= 100; k++) a[i - 1][j - 1][k - 1] = (i * 31 + j * 17 + k * 7) % 101;
  }
}
let sum = 0;
for (let p = 1; p <= 10; p++) {
  for (let i = 0; i < 100; i++) {
    const plane = a[i];
    for (let j = 1; j < 100; j++) {
      const row = plane[j];
      for (let k = 1; k < 100; k++) row[k] = (row[k] + plane[j - 1][k] + row[k - 1]) % 1000;
    }
  }
  for (let i = 0; i < 100; i++) {
    for (let j = 0; j < 100; j++) {
      for (let k = 0; k < 100; k++) sum += a[i][j][k];
    }
  }
}
report("array_nested", t, sum === 4905893027);
