// bench/oop.prg in JavaScript: method calls (one calling its parent's through super), field reads and assignments in a
// loop of 3,000,000.
const { report } = require("./bench.js");

class TBase {
  constructor() { this.total = 0; }
  add(x) { this.total += x; }
}
class TCounter extends TBase {
  constructor() {
    super();
    this.n = 0;
  }
  add(x) { super.add(x); }
  result() { return this.total + this.n; }
}

const t = performance.now();
const o = new TCounter();
for (let i = 1; i <= 3000000; i++) {
  o.add(i % 7);
  o.n = o.n + 1;
}
report("oop", t, o.result() === 11999997);
