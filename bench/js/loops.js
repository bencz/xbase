// bench/loops.prg in JavaScript: the sum of the first 500,000,000 odd numbers. The sum passes 2^53, past which a
// number (a double) is no longer exact: BigInt keeps it exact, as the xBase integers are.
const { report } = require("./bench.js");
const t = performance.now();
let n = 0n;
for (let i = 1n; i <= 500000000n; i++) n = n + i * 2n - 1n;
report("loops", t, n === 250000000000000000n);
