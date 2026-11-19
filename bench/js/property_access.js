// bench/property_access.prg in JavaScript: 3,000,000 rounds on an object made from its class's name (as the .prg's
// macro does) of field reads and assignments (=, += and ++), a computed getter and a setter, and the same again
// through a local alias of the object (the .prg's WITH OBJECT).
const { report } = require("./bench.js");

class TAccount {
  constructor() {
    this.nBalance = 0;
    this.nCount = 0;
    this.nLimit = 0;
    this.cLast = "";
  }
  get Doubled() { return this.nBalance * 2 - this.nCount - 2; }
  set Limit(x) { this.nLimit = x * 10; }
}

const classes = { TAccount };
const t = performance.now();
const o = new classes["TAccount"]();
let n = 0;
for (let i = 1; i <= 3000000; i++) {
  o.nBalance += 2;
  o.nCount++;
  o.cLast = "d";
  n += o.nBalance - o.nCount + o.Doubled;
  o.Limit = i % 10;
  const w = o;
  w.nBalance -= 1;
  n += w.nLimit + w.cLast.length;
}
// round i reads nBalance i + 1, nCount i, Doubled i, then nLimit 10 * ( i % 10 ) and the length of cLast 1: it adds
// i + 1 + 10 * ( i % 10 ) + 1
report("property_access", t, n === 4500001500000 + 10 * 13500000 + 2 * 3000000 && o.nBalance === 3000000);
