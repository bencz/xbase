// bench/aeval_heval.prg in JavaScript: forEach() over an array of 1,000,000 numbers and over a Map of 200,000
// entries, the callback adding to a captured local; 20 rounds.
const { report } = require("./bench.js");

const a = new Array(1000000);
const h = new Map();
for (let i = 0; i < a.length; i++) a[i] = (i + 1) % 777;
for (let i = 1; i <= 200000; i++) h.set(i * 3, i % 555);
const t = performance.now();
let n = 0;
for (let r = 1; r <= 20; r++) {
  a.forEach((x, i) => { n += x + ((i + 1) % 1000 === 0 ? r : 0); });
  h.forEach((v, k) => { n += v + (k % 3000 === 0 ? 1 : 0); });
}
report("aeval_heval", t, n === 8867500260);
