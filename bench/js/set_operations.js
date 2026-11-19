// bench/set_operations.prg in JavaScript: sets as Maps (each key its own value): two sets of 200,000 draws each from
// 0 .. 399,999, their union, intersection, difference and symmetric difference built as new Maps; 2 rounds with new
// members.
const { report } = require("./bench.js");

let seed = 777;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

const t = performance.now();
let check = 0, hA, hB, union, inter, diff, sym;
for (let r = 1; r <= 2; r++) {
  hA = new Map();
  hB = new Map();
  for (let i = 1; i <= 200000; i++) {
    let k = rand(400000);
    hA.set(k, k);
    k = rand(400000);
    hB.set(k, k);
  }
  union = new Map();
  inter = new Map();
  diff = new Map();
  sym = new Map();
  for (const k of hA.values()) {
    union.set(k, k);
    if (hB.has(k)) inter.set(k, k);
    else {
      diff.set(k, k);
      sym.set(k, k);
    }
  }
  for (const k of hB.values()) {
    union.set(k, k);
    if (!hA.has(k)) sym.set(k, k);
  }
  check += hA.size + hB.size * 2 + union.size * 3 + inter.size * 5 + diff.size * 7 + sym.size * 11;
}
report("set_operations", t, check === 8609885 && union.size === 252664 && union.size === inter.size + sym.size &&
  hA.size === inter.size + diff.size);
