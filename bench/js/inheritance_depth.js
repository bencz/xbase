// bench/inheritance_depth.prg in JavaScript: a line of 8 classes, each extending the one before: 4,000,000 rounds, on
// an object of the last made from its class's name (as the .prg's macro does), of a method each class overrides and
// that calls super's down to the first (8 calls), a method only the first class has, and fields set by the first, a
// middle and the last class (read, and the first one assigned).
const { report } = require("./bench.js");

class TL0 {
  constructor() { this.v0 = 0; }
  depth() { return 1; }
  base() { return this.v0 + 100; }
}
class TL1 extends TL0 {
  constructor() { super(); this.v1 = 1; }
  depth() { return super.depth() + 2; }
}
class TL2 extends TL1 {
  constructor() { super(); this.v2 = 2; }
  depth() { return super.depth() + 3; }
}
class TL3 extends TL2 {
  constructor() { super(); this.v3 = 3; }
  depth() { return super.depth() + 4; }
}
class TL4 extends TL3 {
  constructor() { super(); this.v4 = 4; }
  depth() { return super.depth() + 5; }
}
class TL5 extends TL4 {
  constructor() { super(); this.v5 = 5; }
  depth() { return super.depth() + 6; }
}
class TL6 extends TL5 {
  constructor() { super(); this.v6 = 6; }
  depth() { return super.depth() + 7; }
}
class TL7 extends TL6 {
  constructor() { super(); this.v7 = 7; }
  depth() { return super.depth() + 8; }
}

const classes = { TL7 };
const t = performance.now();
const o = new classes["TL7"]();
let n = 0;
for (let i = 1; i <= 4000000; i++) {
  n += o.depth() + o.base() + o.v0 + o.v4 + o.v7;
  o.v0 = i % 3;
}
// o.depth() is 8 + 7 + ... + 1 = 36, o.base() is o.v0 + 100, o.v4 + o.v7 is 11; o.v0 is ( i - 1 ) % 3, 0 at first
report("inheritance_depth", t, n === 4000000 * (36 + 100 + 11) + 2 * 3999999);
