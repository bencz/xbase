// bench/pivot_crosstab.prg in JavaScript: a pivot (crosstab) of 2,000,000 sales records [seller, date, amount] made
// from a fixed seed (60 sellers, the days of 2025, as Date values in UTC), then timed: the amounts summed per seller
// and month in a Map of rows, and per weekday and month; row and column totals, the sellers sorted, and the table
// rendered as text (padEnd, padStart as Str() shows integers) with a totals line.
const { report } = require("./bench.js");

const DAY = 86400000;
let seed = 16180;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};
const str = (n, w) => String(n).padStart(w, " ");

const base = Date.UTC(2025, 0, 1);
const sales = new Array(2000000);
for (let i = 0; i < sales.length; i++) {
  sales[i] = ["SELLER" + String(rand(60) + 1).padStart(3, "0"), new Date(base + rand(365) * DAY), rand(50000) + 1];
}
const t = performance.now();
const rows = new Map(), col = new Array(12).fill(0), dow = new Array(7);
for (let i = 0; i < 7; i++) dow[i] = new Array(12).fill(0);
for (const r of sales) {
  let a = rows.get(r[0]);
  if (a === undefined) {
    a = new Array(12).fill(0);
    rows.set(r[0], a);
  }
  const month = r[1].getUTCMonth();
  a[month] += r[2];
  dow[r[1].getUTCDay()][month] += r[2];
}
let tab = "", best, bestN = 0, s07, grand = 0, sat = 0;
for (const k of [...rows.keys()].sort()) {
  const a = rows.get(k);
  let line = k.padEnd(10, " "), row = 0;
  for (let i = 0; i < 12; i++) {
    line += str(a[i], 11);
    row += a[i];
    col[i] += a[i];
  }
  line += str(row, 13);
  if (row > bestN) {
    best = k;
    bestN = row;
  }
  if (k === "SELLER007") s07 = line;
  tab += line + "\n";
}
let line = "TOTAL".padEnd(10, " ");
for (let i = 0; i < 12; i++) {
  line += str(col[i], 11);
  grand += col[i];
}
tab += line + str(grand, 13) + "\n";
dow[6].forEach((x) => (sat += x));
const ok = grand === 50028051852 && best === "SELLER015" && bestN === 844241139 && col[0] === 4275964399 &&
  col[1] === 3837132322 && col[11] === 4246237533 && dow[0][0] === 547728957 && sat === 7111132032 &&
  tab.length === 61 * (10 + 12 * 11 + 13 + 1) && s07 === "SELLER007 " +
    "   71382096   62515370   68284183   68152167   70863968   69201960" +
    "   71447433   71672784   67917922   68878748   65718083   68477906    824512620";
report("pivot_crosstab", t, ok);
