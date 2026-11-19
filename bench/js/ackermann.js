// bench/ackermann.prg in JavaScript: deep recursion, Ackermann's function A(3, 9) (11 million calls, 4093 levels
// deep), 3 times.
const { report } = require("./bench.js");

function ack(m, n) {
  if (m === 0) return n + 1;
  if (n === 0) return ack(m - 1, 1);
  return ack(m - 1, ack(m, n - 1));
}

const t = performance.now();
let ok = true;
for (let rep = 1; rep <= 3; rep++) ok = ok && ack(3, 9) === 4093;
report("ackermann", t, ok);
