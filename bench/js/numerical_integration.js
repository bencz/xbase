// bench/numerical_integration.prg in JavaScript: Simpson's rule over 1,000,000 intervals for four integrals whose
// values are known (4 / (1 + x^2) and Math.sqrt(x) on [0, 1], Math.exp(-x^2) on [0, 1], 1 / x on [1, 2]), each
// function a closure, 3 times.
const { report } = require("./bench.js");

// The integral of f from a to b by Simpson's rule over n intervals (n even).
function simpson(f, a, b, n) {
  const h = (b - a) / n;
  let s = f(a) + f(b);
  for (let i = 1; i <= n - 1; i++) s = s + ((i & 1) === 1 ? 4.0 : 2.0) * f(a + i * h);
  return (s * h) / 3.0;
}

const t = performance.now();
const n = 1000000;
let ok = true;
for (let rep = 1; rep <= 3; rep++) {
  const r1 = simpson((x) => 4.0 / (1.0 + x * x), 0.0, 1.0, n);
  const r2 = simpson((x) => Math.exp(-(x * x)), 0.0, 1.0, n);
  const r3 = simpson((x) => 1.0 / x, 1.0, 2.0, n);
  const r4 = simpson((x) => Math.sqrt(x), 0.0, 1.0, n);
  ok = ok && Math.abs(r1 - 3.141592653589793) < 0.000000000001 && Math.abs(r2 - 0.746824132812427) < 0.000000000001 &&
    Math.abs(r3 - 0.6931471805599453) < 0.000000000001 && Math.abs(r4 - 2.0 / 3.0) < 0.000000001;
}
report("numerical_integration", t, ok);
