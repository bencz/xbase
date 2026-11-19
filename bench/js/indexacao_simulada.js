// bench/indexacao_simulada.prg in JavaScript: an index kept in memory, as an order is: 150,000 entries (an 8-digit
// key and the record number, one string each) made from a fixed seed, then timed: sorted with the library's sort,
// 100,000 seeks by binary search (the first entry not below the key, compared over the key's length; a hit when its
// key is the one sought), 3,000 entries inserted in place (binary search and splice), and the order checked.
const { report } = require("./bench.js");

let seed = 99991;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};
const strZero = (n, w) => String(n).padStart(w, "0");

// The first position whose entry is not below k, compared over k's length as xBase's < does with SET EXACT OFF
// (a.length when there is none).
function lowerBound(a, k) {
  let lo = 0, hi = a.length;
  while (lo < hi) {
    const m = Math.trunc((lo + hi) / 2);
    if (a[m].substring(0, k.length) < k) lo = m + 1;
    else hi = m;
  }
  return lo;
}

const recs = 150000;
const a = new Array(recs);
for (let i = 1; i <= recs; i++) a[i - 1] = "C" + strZero(rand(10000000), 8) + strZero(i, 6);
const t = performance.now();
a.sort();
let hits = 0, sum = 0, chk = 0, ok = true;
for (let i = 1; i <= 100000; i++) {
  const k = "C" + strZero(rand(10000000), 8);
  const p = lowerBound(a, k);
  if (p < a.length && a[p].substring(0, 9) === k) {
    hits++;
    sum += Number(a[p].slice(-6));
  }
}
for (let i = 1; i <= 3000; i++) {
  const k = "C" + strZero(rand(10000000), 8) + strZero(recs + i, 6);
  a.splice(lowerBound(a, k), 0, k);
}
for (let i = 1; i < a.length; i++) ok = ok && a[i - 1] < a[i];
for (let i = 0; i < a.length; i += 997) chk = (chk * 31 + Number(a[i].slice(-6))) % 1000000007;
ok = ok && hits === 1426 && sum === 107771969 && chk === 409559053 && a[0] === "C00000040027737" &&
  a[a.length - 1] === "C09999947069110";
report("indexacao_simulada", t, ok);
