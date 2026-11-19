// bench/tak.prg in JavaScript: recursive calls with three arguments, the Takeuchi function Tak(24, 16, 8), 10 times.
const { report } = require("./bench.js");
const tak = (x, y, z) => (y < x ? tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y)) : z);
const t = performance.now();
let ok = true;
for (let rep = 1; rep <= 10; rep++) ok = ok && tak(24, 16, 8) === 9;
report("tak", t, ok);
