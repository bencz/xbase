// bench/short_lived_objects.prg in JavaScript: objects that die at once, 2,000,000 small vectors of a class, each added
// to a running sum by a method that returns a new vector, so three objects are made and as many dropped per step.
const { report } = require("./bench.js");

class TVec {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
  add(o) { return new TVec(this.x + o.x, this.y + o.y); }
  scaled(k) { return new TVec(this.x * k, this.y * k); }
  dot(o) { return this.x * o.x + this.y * o.y; }
}

const t = performance.now();
let s = new TVec(0, 0), n = 0;
for (let i = 1; i <= 2000000; i++) {
  const v = new TVec(i % 10, (i % 7) - 3);
  s = s.add(v);
  n += v.dot(s.scaled(0)) + v.x;
}
report("short_lived_objects", t, s.x === 9000000 && s.y === -3 && n === 9000000);
