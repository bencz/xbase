// bench/julia_set.prg in JavaScript: the Julia set of c = -0.8 + 0.156i, escape times over a 1,600 x 1,200 grid of
// [-1.5, 1.5] x [-1, 1], at most 255 iterations per point, counted in a histogram of 256 bins folded into a checksum.
const { report } = require("./bench.js");
const t = performance.now();
const w = 1600, h = 1200, maxIter = 255, cr = -0.8, ci = 0.156;
const hist = new Array(maxIter + 1).fill(0);
let total = 0, cs = 0;
for (let y = 0; y <= h - 1; y++) {
  for (let x = 0; x <= w - 1; x++) {
    let zr = (x * 3.0) / w - 1.5;
    let zi = (y * 2.0) / h - 1.0;
    let k = 0;
    while (k < maxIter) {
      const zr2 = zr * zr;
      const zi2 = zi * zi;
      if (zr2 + zi2 > 4.0) break;
      zi = 2.0 * zr * zi + ci;
      zr = zr2 - zi2 + cr;
      k++;
    }
    hist[k]++;
    total += k;
  }
}
for (let k = 0; k <= maxIter; k++) cs = (cs * 31 + hist[k]) % 2147483648;
report("julia_set", t, total === 69243973 && cs === 2105097618);
