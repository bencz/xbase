// bench/closure_capture.prg in JavaScript: 500,000 counters made by a function, each a closure holding its own copy of
// a local and a parameter, each called 4 times; then 2,000,000 calls of closures that share one captured local and
// change it.
const { report } = require("./bench.js");

function counter(step) {
  let count = 0;
  return (x) => (count += step + x);
}

const t = performance.now();
const a = new Array(500000);
let n = 0, shared = 0;
for (let i = 0; i < a.length; i++) a[i] = counter((i + 1) % 10);
for (let j = 1; j <= 4; j++) for (const b of a) n += b(j);
const inc = (x) => (shared += x);
const read = () => shared % 1000;
for (let i = 1; i <= 2000000; i++) {
  inc(i % 7);
  n += read();
}
report("closure_capture", t, n === 1031500997 && shared === 5999997);
