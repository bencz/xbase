// bench/black_scholes.prg in JavaScript: Black-Scholes prices of 500,000 European options (a call and a put each),
// their inputs from a linear congruential generator, the normal distribution by its polynomial approximation,
// Math.exp(), Math.log() and Math.sqrt(), the sum of the prices. seed * 1103515245 passes 2^53; Math.imul keeps the
// low 32 bits of the product exact, and only the low 31 are kept.
const { report } = require("./bench.js");

// The cumulative normal distribution (the polynomial approximation of Abramowitz and Stegun, 26.2.17).
function cnd(d) {
  const k = 1.0 / (1.0 + 0.2316419 * Math.abs(d));
  const w = 1.0 - 0.3989422804014327 * Math.exp(-0.5 * d * d) *
    k * (0.319381530 + k * (-0.356563782 + k * (1.781477937 + k * (-1.821255978 + k * 1.330274429))));
  return d < 0.0 ? 1.0 - w : w;
}

const t = performance.now();
const n = 500000, r = 0.02, v = 0.30;
let seed = 12345, sumCall = 0.0, sumPut = 0.0;
for (let i = 1; i <= n; i++) {
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
  const s = 5.0 + (25.0 * seed) / 2147483648.0;
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
  const x = 1.0 + (99.0 * seed) / 2147483648.0;
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7fffffff;
  const tm = 0.25 + (9.75 * seed) / 2147483648.0;
  const vst = v * Math.sqrt(tm);
  const d1 = (Math.log(s / x) + (r + v * v * 0.5) * tm) / vst;
  const d2 = d1 - vst;
  const disc = x * Math.exp(-r * tm);
  sumCall += s * cnd(d1) - disc * cnd(d2);
  sumPut += disc * cnd(-d2) - s * cnd(-d1);
}
const ok = Math.abs(sumCall / 1494483.92385431 - 1) < 0.000000001 &&
  Math.abs(sumPut / 15576648.71605097 - 1) < 0.000000001;
report("black_scholes", t, ok);
