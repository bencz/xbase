// bench/radix_sort.prg in JavaScript: least-significant-digit radix sort, four passes of one byte each, of 300,000
// 31-bit integers from a linear congruential generator, between two arrays, 10 times with different seeds; the order
// and a weighted checksum are checked. seed * 1103515245 passes 2^53; Math.imul keeps the low 32 bits of the product
// exact, and only the low 31 are kept. The checksum passes 2^53: a round's part stays below it (300,000 * 8 * 2^31)
// and the total is a BigInt. The arrays keep the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");
const t = performance.now();
const n = 300000, cnt = new Array(257);
let a = new Array(n + 1), b = new Array(n + 1), ok = true, sum = 0n;
for (let rep = 1; rep <= 10; rep++) {
  let seed = rep + 30;
  for (let i = 1; i <= n; i++) {
    seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
    a[i] = seed;
  }
  for (let d = 0; d <= 3; d++) {
    const shift = d * 8;
    cnt.fill(0);
    for (let i = 1; i <= n; i++) cnt[((a[i] >> shift) & 0xff) + 1]++;
    let pos = 1;
    for (let k = 1; k <= 256; k++) {
      const x = cnt[k];
      cnt[k] = pos;
      pos += x;
    }
    for (let i = 1; i <= n; i++) {
      const x = a[i];
      const k = ((x >> shift) & 0xff) + 1;
      b[cnt[k]] = x;
      cnt[k]++;
    }
    const tmp = a;
    a = b;
    b = tmp;
  }
  for (let i = 2; i <= n; i++) if (a[i - 1] > a[i]) ok = false;
  let part = 0;
  for (let i = 1; i <= n; i++) part += a[i] * ((i & 7) + 1);
  sum += BigInt(part);
}
report("radix_sort", t, ok && sum === 14501540638159929n);
