// bench/merge_sort.prg in JavaScript: recursive top-down merge sort of 300,000 integers from a linear congruential
// generator, through one work array, 3 times with different seeds; the order and a weighted checksum of the sorted
// values are checked (its terms are positive and the total is below 2^53, so the sum stays exact). The arrays keep
// the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");

// Sorts a[lo .. hi]; w is the work array the halves merge through.
function mergeSort(a, w, lo, hi) {
  if (hi - lo < 1) return;
  const mid = Math.floor((lo + hi) / 2);
  mergeSort(a, w, lo, mid);
  mergeSort(a, w, mid + 1, hi);
  let i = lo, j = mid + 1, k = lo;
  while (i <= mid && j <= hi) {
    if (a[j] < a[i]) {
      w[k] = a[j];
      j++;
    } else {
      w[k] = a[i];
      i++;
    }
    k++;
  }
  while (i <= mid) {
    w[k] = a[i];
    i++;
    k++;
  }
  while (j <= hi) {
    w[k] = a[j];
    j++;
    k++;
  }
  for (k = lo; k <= hi; k++) a[k] = w[k];
}

const t = performance.now();
const n = 300000, a = new Array(n + 1), w = new Array(n + 1);
let ok = true, sum = 0;
for (let rep = 1; rep <= 3; rep++) {
  let seed = rep;
  for (let i = 1; i <= n; i++) {
    seed = (seed * 16807) % 2147483647;
    a[i] = seed;
  }
  mergeSort(a, w, 1, n);
  for (let i = 2; i <= n; i++) if (a[i - 1] > a[i]) ok = false;
  for (let i = 1; i <= n; i++) sum += a[i] * ((i % 8) + 1);
}
report("merge_sort", t, ok && sum === 4349496350073402);
