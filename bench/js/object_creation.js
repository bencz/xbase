// bench/object_creation.prg in JavaScript: 600,000 objects of a class with five fields (numbers, a string and an
// array, which each object gets a copy of) made with a constructor that sets two of them, half of them of a subclass
// with two more fields; one in 100 kept in an array, the rest left to the collector.
const { report } = require("./bench.js");

// An xBase date is a value: one Date that no object changes stands for it.
const DUE = new Date(2026, 11, 31);

class TOrder {
  constructor(nId, cCustomer) {
    this.nId = 0;
    this.cCustomer = "";
    this.nTotal = 0;
    this.cStatus = "new";
    this.aLines = [];
    this.nId = nId;
    this.cCustomer = cCustomer;
  }
}
class TBigOrder extends TOrder {
  constructor(nId, cCustomer) {
    super(nId, cCustomer);
    this.nDiscount = 0;
    this.dDue = DUE;
  }
}

const t = performance.now();
const aKeep = [];
let n = 0;
for (let i = 1; i <= 600000; i++) {
  let o;
  if (i % 2 === 0) {
    o = new TOrder(i, "c" + String(i % 1000).padStart(3, "0"));
  } else {
    o = new TBigOrder(i, "c000");
    o.nDiscount = 5;
  }
  o.aLines.push(i % 7);
  if (i % 100 === 0) aKeep.push(o);
}
for (const o of aKeep) n += o.nId + o.nTotal + o.aLines.length + o.aLines[0] + o.cCustomer.length;
// the kept ones: ids 100, 200, ..., 600,000 (all even: TOrder), nTotal 0, one line of i % 7, a 4-character name;
// 17999 is the sum of i % 7 over those ids
report("object_creation", t, aKeep.length === 6000 && n === 1800300000 + 6000 * 5 + 17999);
