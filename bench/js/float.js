// bench/float.prg in JavaScript: Mandelbrot iterations over a 250 x 250 grid, at most 100 per point, 6 times.
const { report } = require("./bench.js");
const t = performance.now();
const size = 250, maxIter = 100;
let total = 0;
for (let rep = 1; rep <= 6; rep++)
  for (let y = 0; y < size; y++) {
    const ci = (y * 2.0) / size - 1.0;
    for (let x = 0; x < size; x++) {
      const cr = (x * 3.0) / size - 2.0;
      let zr = 0.0, zi = 0.0, k = 0;
      while (k < maxIter) {
        const zr2 = zr * zr, zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) break;
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
        k++;
      }
      total += k;
    }
  }
report("float", t, total === 11391384);
