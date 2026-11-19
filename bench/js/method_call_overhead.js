// bench/method_call_overhead.prg in JavaScript: 5,000,000 rounds of method calls with no argument, one and three
// arguments, a one-line method and a method that calls another through this; on an object made with `new TCalc()`,
// then on one made from its class's name (as the .prg's macro does).
const { report } = require("./bench.js");

class TCalc {
  constructor() { this.nBase = 1; }
  zero() { return this.nBase; }
  one(x) { return x; }
  three(x, y, z) { return x + y + z - 5; }
  twice(x) { return x + x; }
  chain(x) { return this.one(x) + this.one(x) + 1; }
}

const classes = { TCalc };
const t = performance.now();
const o = new TCalc(), u = new classes["TCalc"]();
let n = 0;
for (let i = 1; i <= 5000000; i++) {
  n += o.zero() + o.one(i) + o.three(i, 2, 3) + o.twice(i) + o.chain(i);
  n += u.zero() + u.one(i) + u.three(i, 2, 3) + u.twice(i) + u.chain(i);
}
// each round adds 2 * ( 6 * i + 2 )
report("method_call_overhead", t, n === 150000050000000);
