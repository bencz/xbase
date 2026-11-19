// bench/relatorio_agregacao.prg in JavaScript: 1,000,000 sales records made from a fixed seed, then timed: aggregated
// by region and by product in Maps keyed by code, the 500 products ordered by amount with the library's sort (ties by
// code), and a report built with the German number format (as Transform's @E pictures show it): a line per region and
// the top 20 products.
const { report } = require("./bench.js");

let seed = 12345;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};
const strZero = (n, w) => String(n).padStart(w, "0");

// Transform( n, "@E ..." ): the number in the picture's width, "." between thousands and "," for the point.
const fmt0 = new Intl.NumberFormat("de-DE", { maximumFractionDigits: 0 });
const fmt2 = new Intl.NumberFormat("de-DE", { minimumFractionDigits: 2, maximumFractionDigits: 2 });
const picture = (f, n, width) => f.format(n).padStart(width, " ");

const sales = new Array(1000000);
for (let i = 0; i < sales.length; i++) {
  sales[i] = ["R" + strZero(rand(8) + 1, 2), "P" + strZero(rand(500) + 1, 4), rand(20) + 1, rand(100000) + 100];
}
const t = performance.now();
const reg = new Map(), prod = new Map();
let total = 0;
for (const r of sales) {
  const x = r[2] * r[3];
  const a = reg.get(r[0]);
  if (a !== undefined) {
    a[0]++;
    a[1] += r[2];
    a[2] += x;
  } else {
    reg.set(r[0], [1, r[2], x]);
  }
  prod.set(r[1], (prod.get(r[1]) ?? 0) + x);
  total += x;
}
const prods = [];
for (const [k, x] of prod) prods.push([k, x]);
prods.sort((p, q) => q[1] - p[1] || (p[0] < q[0] ? -1 : p[0] > q[0] ? 1 : 0));
let rep = "";
for (const k of [...reg.keys()].sort()) {
  const a = reg.get(k);
  rep += k + picture(fmt0, a[0], 9) + picture(fmt0, a[1], 10) + picture(fmt2, a[2] / 100, 18) + "\n";
}
for (let i = 0; i < 20; i++) rep += prods[i][0] + picture(fmt2, prods[i][1] / 100, 18) + "\n";
const lines = rep.split("\n");
const ok = total === 525748851783 && rep.length === 8 * 41 + 20 * 24 && prod.size === 500 &&
  lines[0] === "R01  125.691 1.320.222    662.846.057,84" && lines[7] === "R08  124.683 1.310.936    656.013.155,38" &&
  lines[8] === "P0303     11.438.921,18" && lines[27] === "P0360     11.021.303,35";
report("relatorio_agregacao", t, ok);
