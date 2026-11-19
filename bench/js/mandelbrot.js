// bench/mandelbrot.prg in JavaScript: the Mandelbrot set as a bitmap (the benchmarks game's form), 1,600 x 1,600
// points, at most 50 iterations each, one bit per point packed into bytes (a row padded to a whole byte), the bytes
// folded into a checksum.
const { report } = require("./bench.js");
const t = performance.now();
const n = 1600;
let cs = 0, nBytes = 0;
for (let y = 0; y <= n - 1; y++) {
  const ci = (2.0 * y) / n - 1.0;
  let bits = 0, nBits = 0;
  for (let x = 0; x <= n - 1; x++) {
    const cr = (2.0 * x) / n - 1.5;
    let zr = 0.0, zi = 0.0, tr = 0.0, ti = 0.0, k = 0;
    while (k < 50 && tr + ti <= 4.0) {
      zi = 2.0 * zr * zi + ci;
      zr = tr - ti + cr;
      tr = zr * zr;
      ti = zi * zi;
      k++;
    }
    bits = (bits << 1) | (tr + ti <= 4.0 ? 1 : 0);
    nBits++;
    if (nBits === 8 || x === n - 1) {
      bits = bits << (8 - nBits);
      cs = (cs * 31 + bits) & 0x7fffffff;
      nBytes++;
      bits = 0;
      nBits = 0;
    }
  }
}
report("mandelbrot", t, cs === 366763831 && nBytes === n * Math.floor((n + 7) / 8));
