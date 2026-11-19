// bench/array_of_hashes.prg in JavaScript: 120,000 records as Maps of four fields made from a fixed-seed generator,
// totals per group read by field name, a count of the records that pass a test, and the array sorted by a field with
// a comparison function.
const { report } = require("./bench.js");

let seed = 31337;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

const t = performance.now();
const a = new Array(120000);
const total = new Array(50).fill(0);
let pass = 0, sum = 0;
for (let i = 0; i < a.length; i++) {
  a[i] = new Map([["id", i + 1], ["name", "N" + String(rand(100000)).padStart(5, "0")], ["score", rand(1000)],
    ["group", rand(50) + 1]]);
}
for (const r of a) {
  total[r.get("group") - 1] += r.get("score");
  if (r.get("score") >= 500 && r.get("name").substring(0, 2) === "N1") pass++;
}
a.sort((x, y) => x.get("score") - y.get("score"));
for (let i = 0; i < a.length; i += 1000) sum += a[i].get("score");
report("array_of_hashes", t, total[0] === 1239678 && total[49] === 1170281 && pass === 5950 && sum === 59429 &&
  a[0].get("score") === 0 && a[119999].get("score") === 999);
