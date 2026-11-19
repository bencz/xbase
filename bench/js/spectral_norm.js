// bench/spectral_norm.prg in JavaScript: the spectral norm of the infinite matrix A(i,j) = 1 / ((i+j)(i+j+1)/2 + i
// + 1) (the benchmarks game's), cut to 1,200 x 1,200, 10 rounds of the power method on A transposed times A, each
// element of A from a function call.
const { report } = require("./bench.js");

// The element of A at the 0-based row i and column j.
const evalA = (i, j) => 1.0 / (Math.floor(((i + j) * (i + j + 1)) / 2) + i + 1);

// v := A * u
function mulAv(n, u, v) {
  for (let i = 0; i < n; i++) {
    let s = 0.0;
    for (let j = 0; j < n; j++) s += evalA(i, j) * u[j];
    v[i] = s;
  }
}

// v := A transposed * u
function mulAtv(n, u, v) {
  for (let i = 0; i < n; i++) {
    let s = 0.0;
    for (let j = 0; j < n; j++) s += evalA(j, i) * u[j];
    v[i] = s;
  }
}

// v := A transposed * A * u, through w
function mulAtAv(n, u, v, w) {
  mulAv(n, u, w);
  mulAtv(n, w, v);
}

const t = performance.now();
const n = 1200;
const u = new Array(n).fill(1.0), v = new Array(n).fill(0.0), w = new Array(n).fill(0.0);
for (let i = 1; i <= 10; i++) {
  mulAtAv(n, u, v, w);
  mulAtAv(n, v, u, w);
}
let vbv = 0.0, vv = 0.0;
for (let i = 0; i < n; i++) {
  vbv += u[i] * v[i];
  vv += v[i] * v[i];
}
const r = Math.sqrt(vbv / vv);
report("spectral_norm", t, Math.abs(r - 1.2742241501) < 0.000000001);
