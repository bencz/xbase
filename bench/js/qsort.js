// bench/qsort.prg in JavaScript: recursive quicksort over 200,000 Park-Miller numbers, 5 times.
const { report } = require("./bench.js");

function qs(a, lo, hi) {
  while (lo < hi) {
    const pivot = a[Math.floor((lo + hi) / 2)];
    let i = lo, j = hi;
    while (i <= j) {
      while (a[i] < pivot) i++;
      while (a[j] > pivot) j--;
      if (i <= j) {
        const tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
        i++;
        j--;
      }
    }
    if (j - lo < hi - i) {
      qs(a, lo, j);
      lo = i;
    } else {
      qs(a, i, hi);
      hi = j;
    }
  }
}

function sortRandom() {
  const n = 200000, a = new Array(n + 1).fill(0); // 1-based, as the xBase array
  let x = 12345, sum = 0, sorted = true;
  for (let i = 1; i <= n; i++) {
    x = (x * 16807) % 2147483647;
    a[i] = x % 1000000;
  }
  qs(a, 1, n);
  for (let i = 2; i <= n; i++) if (a[i - 1] > a[i]) sorted = false;
  for (let i = 1; i <= n; i += 1000) sum += a[i];
  return sorted && a[1] === 6 && a[100000] === 500294 && a[n] === 999998 && sum === 99556483;
}

const t = performance.now();
let ok = true;
for (let rep = 1; rep <= 5; rep++) ok = ok && sortRandom();
report("qsort", t, ok);
