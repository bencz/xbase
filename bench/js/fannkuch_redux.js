// bench/fannkuch_redux.prg in JavaScript: small integer arrays permuted and reversed in place, the pancake flips of
// every permutation of 9 elements (the checksum and the most flips), 4 times. The arrays keep the .prg's 1-based
// indexes (element 0 unused).
const { report } = require("./bench.js");
const t = performance.now();
const n = 9;
let ok = true;
for (let rep = 1; rep <= 4; rep++) {
  const perm = new Array(n + 1).fill(0), perm1 = new Array(n + 2).fill(0), count = new Array(n + 1).fill(0);
  for (let i = 1; i <= n; i++) perm1[i] = i - 1;
  let checksum = 0, maxFlips = 0, permCount = 0, r = n, done = false;
  while (!done) {
    while (r !== 1) {
      count[r] = r;
      r--;
    }
    for (let i = 1; i <= n; i++) perm[i] = perm1[i];
    let flips = 0, k = perm[1];
    while (k !== 0) {
      let i = 1, j = k + 1;
      while (i < j) {
        const x = perm[i];
        perm[i] = perm[j];
        perm[j] = x;
        i++;
        j--;
      }
      flips++;
      k = perm[1];
    }
    maxFlips = Math.max(maxFlips, flips);
    checksum += (permCount & 1) === 0 ? flips : -flips;
    for (;;) {
      if (r === n) {
        done = true;
        break;
      }
      const perm0 = perm1[1];
      for (let i = 1; i <= r; i++) perm1[i] = perm1[i + 1];
      perm1[r + 1] = perm0;
      count[r + 1]--;
      if (count[r + 1] > 0) break;
      r++;
    }
    permCount++;
  }
  ok = ok && checksum === 8629 && maxFlips === 30;
}
report("fannkuch_redux", t, ok);
