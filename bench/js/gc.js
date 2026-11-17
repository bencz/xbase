// bench/gc.prg in JavaScript: 40,000 slots holding strings of 16 to 2,000 characters and small records of them,
// replaced at random 3,000,000 times, while 20,000 records that live to the end are kept beside them.
const { report } = require("./bench.js");
let seed = 7;
function rand(n) {
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff; // modulo 2^31 on the low 32 bits: exact
  return Math.floor(seed / 65536) % n;
}
const t = performance.now();
const slots = new Array(40000).fill(null), kept = [];
for (let i = 1; i <= 3000000; i++) {
  const n = rand(slots.length);
  let x;
  if (rand(4) === 0)
    x = [String.fromCharCode(65 + (i % 26)).repeat(16 + rand(200)), i, " ".repeat(rand(100))];
  else x = String.fromCharCode(97 + (i % 26)).repeat(16 + rand(2000));
  slots[n] = x;
  if (i % 150 === 0) kept.push([i, String(i).padStart(10), "k".repeat(64)]);
}
let check = 0;
for (const x of slots) {
  if (typeof x === "string") check += x.length;
  else if (Array.isArray(x)) check += x[0].length + x[2].length;
}
report("gc", t, kept.length === 20000 && check > 0);
