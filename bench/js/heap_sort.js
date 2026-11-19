// bench/heap_sort.prg in JavaScript: heap sort in place (a binary max-heap built bottom-up, then sifted down) of
// 300,000 integers from a linear congruential generator, 3 times with different seeds; the order and a weighted
// checksum are checked (its terms are positive and the total is below 2^53, so the sum stays exact). The array keeps
// the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");

// Moves a[i] down the heap a[1 .. n] until neither child is larger.
function siftDown(a, i, n) {
  const x = a[i];
  let c;
  while ((c = i * 2) <= n) {
    if (c < n && a[c + 1] > a[c]) c++;
    if (a[c] <= x) break;
    a[i] = a[c];
    i = c;
  }
  a[i] = x;
}

const t = performance.now();
const n = 300000, a = new Array(n + 1);
let ok = true, sum = 0;
for (let rep = 1; rep <= 3; rep++) {
  let seed = rep + 10;
  for (let i = 1; i <= n; i++) {
    seed = (seed * 16807) % 2147483647;
    a[i] = seed;
  }
  for (let i = Math.floor(n / 2); i >= 1; i--) siftDown(a, i, n);
  for (let i = n; i >= 2; i--) {
    const x = a[1];
    a[1] = a[i];
    a[i] = x;
    siftDown(a, 1, i - 1);
  }
  for (let i = 2; i <= n; i++) if (a[i - 1] > a[i]) ok = false;
  for (let i = 1; i <= n; i++) sum += a[i] * ((i % 8) + 1);
}
report("heap_sort", t, ok && sum === 4351668891394344);
