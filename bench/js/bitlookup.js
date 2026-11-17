// bench/bitlookup.prg in JavaScript: the bitwise AND from a table of the ANDs of two nibbles, IOR and XOR written out
// as the .prg's macros expand them (7 calls of and() a pair; the JIT may inline them), for every pair a, b in 0 .. 999.
const { report } = require("./bench.js");
const table = new Uint16Array(256);
for (let a = 0; a < 16; a++) for (let b = 0; b < 16; b++) table[a * 16 + b] = a & b;
function and(a, b) {
  let r = 0;
  for (let i = 0; i < 16; i += 4) {
    r = Math.trunc(r / 16) + table[(a % 16) * 16 + (b % 16)] * 4096;
    a = Math.trunc(a / 16);
    b = Math.trunc(b / 16);
  }
  return r;
}
const t = performance.now();
let ok = true;
outer: for (let a = 0; a < 1000; a++)
  for (let b = 0; b < 1000; b++)
    if (and(a, b) !== (a & b) ||
        a + b - 2 * and(a, b) + and(a, b) - 2 * and(a + b - 2 * and(a, b), and(a, b)) !== (a | b) ||
        a + b - 2 * and(a, b) !== (a ^ b)) {
      ok = false;
      break outer;
    }
report("bitlookup", t, ok);
