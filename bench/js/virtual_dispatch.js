// bench/virtual_dispatch.prg in JavaScript: 1,000 shapes of 6 classes that override area() and perimeter() of their
// parent TShape (made from a fixed seed, in random order), gone through 1,000 times: each shape's area() and
// perimeter() from one call site, and measure(), which TShape alone has and which calls the overriding area() through
// this.
const { report } = require("./bench.js");

let seed = 6502;
const rand = (n) => {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
};

class TShape {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
  area() { return 0; }
  perimeter() { return 0; }
  measure() { return this.area() + 1; }
}
class TCircle extends TShape {
  area() { return 3 * this.x * this.x; }
  perimeter() { return 6 * this.x; }
}
class TSquare extends TShape {
  area() { return this.x * this.x; }
  perimeter() { return 4 * this.x; }
}
class TRect extends TShape {
  area() { return this.x * this.y; }
  perimeter() { return 2 * (this.x + this.y); }
}
class TTriangle extends TShape {
  area() { return (this.x * this.y * 2) / 2; }
  perimeter() { return 3 * this.x; }
}
class THexagon extends TShape {
  area() { return 6 * this.x * this.x; }
  perimeter() { return 6 * this.x; }
}
class TEllipse extends TShape {
  area() { return 3 * this.x * this.y; }
  perimeter() { return 3 * (this.x + this.y); }
}

const aClasses = [TCircle, TSquare, TRect, TTriangle, THexagon, TEllipse];
const aShapes = new Array(1000);
for (let i = 0; i < aShapes.length; i++) {
  const C = aClasses[rand(6)];
  const x = rand(100) + 1;
  aShapes[i] = new C(x, rand(100) + 1);
}
const t = performance.now();
let nArea = 0, nPer = 0, nM = 0;
for (let i = 1; i <= 1000; i++) {
  for (const o of aShapes) {
    nArea += o.area();
    nPer += o.perimeter();
    nM += o.measure();
  }
}
report("virtual_dispatch", t, nArea === 1000 * 7844067 && nPer === 1000 * 245962 && nM === 1000 * (7844067 + 1000));
