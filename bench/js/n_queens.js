// bench/n_queens.prg in JavaScript: backtracking over arrays of booleans, the solutions of the 12 queens problem
// (taken columns and diagonals), 3 times. The arrays keep the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");

// The solutions with queens on rows 1 to row - 1 as col and the diagonals d1 and d2 say.
function place(row, n, col, d1, d2) {
  if (row > n) return 1;
  let count = 0;
  for (let c = 1; c <= n; c++) {
    if (!col[c] && !d1[row + c - 1] && !d2[row - c + n]) {
      col[c] = true;
      d1[row + c - 1] = true;
      d2[row - c + n] = true;
      count += place(row + 1, n, col, d1, d2);
      col[c] = false;
      d1[row + c - 1] = false;
      d2[row - c + n] = false;
    }
  }
  return count;
}

const t = performance.now();
const n = 12;
let ok = true;
for (let rep = 1; rep <= 3; rep++) {
  ok = ok && place(1, n, new Array(n + 1).fill(false), new Array(2 * n).fill(false), new Array(2 * n).fill(false)) ===
    14200;
}
report("n_queens", t, ok);
