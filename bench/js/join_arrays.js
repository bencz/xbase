// bench/join_arrays.prg in JavaScript: joins of two arrays of records, 20,000 customers [id, name, segment, city] and
// 500,000 orders [id, customer, amount] made from a fixed seed (a third of the orders name a customer that exists).
// Timed: a hash join (the customers in a Map by id, each order looked up; amounts summed per segment, orders counted
// per city in a Map), then a sort-merge join (the orders' customer and position packed in one integer key, sorted with
// the library's sort, merged with the customers in id order; the amounts weighted by segment).
const { report } = require("./bench.js");

let seed = 2718;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

const cust = new Array(20000), ord = new Array(500000);
for (let i = 1; i <= cust.length; i++) {
  cust[i - 1] = [i * 3, "Customer " + i, rand(5) + 1, "CITY" + String(rand(100) + 1).padStart(3, "0")];
}
for (let i = 1; i <= ord.length; i++) ord[i - 1] = [i, rand(60000) + 1, rand(100000) + 1];
const t = performance.now();

const hCust = new Map(), seg = [0, 0, 0, 0, 0], city = new Map();
let matched = 0, best, bestN = 0;
for (const c of cust) hCust.set(c[0], c);
for (const o of ord) {
  const c = hCust.get(o[1]);
  if (c !== undefined) {
    seg[c[2] - 1] += o[2];
    city.set(c[3], (city.get(c[3]) ?? 0) + 1);
    matched++;
  }
}
for (const [k, n] of city) {
  if (n > bestN || (n === bestN && k < best)) {
    best = k;
    bestN = n;
  }
}

const keys = new Array(ord.length);
for (let i = 1; i <= ord.length; i++) keys[i - 1] = ord[i - 1][1] * 1000000 + i;
keys.sort((p, q) => p - q);
let pos = 0, w = 0, merged = 0;
for (const k of keys) {
  const o = ord[(k % 1000000) - 1];
  while (pos < cust.length && cust[pos][0] < o[1]) pos++;
  if (pos < cust.length && cust[pos][0] === o[1]) {
    w += o[2] * cust[pos][2];
    merged++;
  }
}

const ok = matched === 166641 && merged === 166641 && w === 25122891088 && seg[0] === 1626943956 &&
  seg[1] === 1648544230 && seg[2] === 1673211091 && seg[3] === 1694374411 && seg[4] === 1680345551 &&
  best === "CITY080" && bestN === 1980 && city.size === 100;
report("join_arrays", t, ok);
