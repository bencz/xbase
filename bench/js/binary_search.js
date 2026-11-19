// bench/binary_search.prg in JavaScript: 1,000,000 keys from a fixed-seed generator looked for by binary search in a
// sorted array of 1,000,000 numbers (every third number), about a third of them found.
const { report } = require("./bench.js");

let seed = 4711;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

const a = new Array(1000000);
for (let i = 0; i < a.length; i++) a[i] = (i + 1) * 3;
const t = performance.now();
let found = 0, posSum = 0;
for (let i = 1; i <= 1000000; i++) {
  const k = rand(3000010);
  let lo = 1, hi = a.length;
  while (lo <= hi) {
    const mid = Math.floor((lo + hi) / 2);
    if (a[mid - 1] < k) lo = mid + 1;
    else if (a[mid - 1] > k) hi = mid - 1;
    else {
      found++;
      posSum += mid;
      break;
    }
  }
}
report("binary_search", t, found === 333181 && posSum === 166512579113);
