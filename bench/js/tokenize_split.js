// bench/tokenize_split.prg in JavaScript: 200,000 comma-separated lines of 6 fields cut with indexOf() and
// substring() into arrays (the .prg's own Split()), the numeric fields read with Number(); 2 rounds.
const { report } = require("./bench.js");

let seed = 64;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

function split(c, sep) {
  const a = [];
  let n;
  while ((n = c.indexOf(sep)) >= 0) {
    a.push(c.substring(0, n));
    c = c.substring(n + 1);
  }
  a.push(c);
  return a;
}

const lines = new Array(200000);
for (let i = 0; i < lines.length; i++) {
  lines[i] = String(rand(100000)) + ",name" + String(i + 1) + "," + String(rand(1000)) + ",," + "z".repeat(rand(12)) +
    ",end";
}
const t = performance.now();
let n = 0;
for (let r = 1; r <= 2; r++) {
  for (const c of lines) {
    const a = split(c, ",");
    n += Number(a[0]) + Number(a[2]) + a.length + a[4].length;
  }
}
report("tokenize_split", t, n === 20190509958);
