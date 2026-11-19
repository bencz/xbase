// bench/counting_sort.prg in JavaScript: counting sort of 1,000,000 keys in 0 .. 65535 (the high bits of a linear
// congruential generator), stable through prefix sums into an output array, 6 times with different seeds; the order
// and a weighted checksum are checked. The arrays keep the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");
const t = performance.now();
const n = 1000000, m = 65536, a = new Array(n + 1), b = new Array(n + 1), cnt = new Array(m + 1);
let ok = true, sum = 0; // the sum stays below 2^53
for (let rep = 1; rep <= 6; rep++) {
  let seed = rep + 20;
  for (let i = 1; i <= n; i++) {
    seed = (seed * 16807) % 2147483647;
    a[i] = Math.floor(seed / 32768);
  }
  cnt.fill(0);
  for (let i = 1; i <= n; i++) cnt[a[i] + 1]++;
  let pos = 1;
  for (let k = 1; k <= m; k++) {
    const c = cnt[k];
    cnt[k] = pos;
    pos += c;
  }
  for (let i = 1; i <= n; i++) {
    const k = a[i] + 1;
    b[cnt[k]] = a[i];
    cnt[k]++;
  }
  for (let i = 2; i <= n; i++) if (b[i - 1] > b[i]) ok = false;
  for (let i = 1; i <= n; i++) sum += b[i] * ((i % 8) + 1);
}
report("counting_sort", t, ok && sum === 884633960242);
