// bench/codeblock_eval.prg in JavaScript: 10,000,000 calls of each of three closures kept in an array (no argument,
// two arguments, one that reads a captured local), the results summed.
const { report } = require("./bench.js");

function blocks(base) {
  return [() => 7, (x, y) => x * 2 + y, (x) => x + base];
}

const t = performance.now();
const b = blocks(3);
let n = 0;
for (let i = 1; i <= 10000000; i++) n += b[0]() + b[1](i % 100, 1) + b[2](i % 10);
report("codeblock_eval", t, n === 1145000000);
