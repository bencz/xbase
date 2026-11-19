// bench/object_churn.prg in JavaScript: 50,000 slots holding objects of three classes that point to other slots'
// objects (cycles included); 700,000 times a slot's object is replaced by a new one, then the graph is walked.
const { report } = require("./bench.js");

let seed = 9;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};
// Str( n ) of the .prg's i % 100: a remainder carries 2 decimals in xBase, so Str() writes it in 13 characters.
const str = (n) => n.toFixed(2).padStart(13, " ");

class TLeaf {
  constructor(n) { this.nValue = n; }
  weight() { return this.nValue; }
}
class TPair extends TLeaf {
  constructor(n, oL, oR) {
    super(n);
    this.oLeft = oL;
    this.oRight = oR;
  }
  weight() { return this.nValue + this.oLeft.nValue + this.oRight.nValue; }
}
class TList extends TLeaf {
  constructor(n, o) {
    super(n);
    this.aItems = [o, n, str(n)];
  }
  weight() { return this.nValue + this.aItems[0].nValue + this.aItems[2].length; }
}

const t = performance.now();
const aSlots = new Array(50000);
for (let i = 0; i < aSlots.length; i++) aSlots[i] = new TLeaf(i + 1);
for (let i = 1; i <= 700000; i++) {
  const k = rand(aSlots.length);
  const nKind = rand(3);
  let o;
  if (nKind === 0) {
    o = new TLeaf(i % 100);
  } else if (nKind === 1) {
    const l = aSlots[rand(aSlots.length)];
    o = new TPair(i % 100, l, aSlots[rand(aSlots.length)]);
  } else {
    o = new TList(i % 100, aSlots[rand(aSlots.length)]);
  }
  aSlots[k] = o;
}
let n = 0;
for (const o of aSlots) n += o.weight();
report("object_churn", t, n === 5162688);
