// bench/fft.prg in JavaScript: the fast Fourier transform (iterative radix 2, in place, on arrays of real and imaginary
// parts): 65,536 complex points from a linear congruential generator, transformed forward and back 10 times. The
// twiddle factors come from Math.cos and Math.sin of 2 pi / 2^k and complex multiplication; the check is the
// spectrum's sum of magnitudes and the round trip's largest error. The generator's product passes 2^53; only its low
// 31 bits are kept, so Math.imul (the low 32 bits of the product, exact) gives the same values as the .prg's 64-bit
// integers.
const { report } = require("./bench.js");

// The transform of re + i im in place; sign -1 forward, 1 backward (unscaled).
function fft(re, im, n, cosT, sinT, sign) {
  let j = 0;
  for (let i = 0; i <= n - 2; i++) {
    if (i < j) {
      let tmp = re[i];
      re[i] = re[j];
      re[j] = tmp;
      tmp = im[i];
      im[i] = im[j];
      im[j] = tmp;
    }
    let k = n >> 1;
    while (k <= j) {
      j -= k;
      k >>= 1;
    }
    j += k;
  }
  for (let len = 2, st = 0; len <= n; len <<= 1, st++) {
    const half = len >> 1, wr0 = cosT[st], wi0 = sign * sinT[st];
    for (let i = 0; i < n; i += len) {
      let wr = 1.0, wi = 0.0;
      for (let j = i; j <= i + half - 1; j++) {
        const k = j + half, xr = re[k] * wr - im[k] * wi, xi = re[k] * wi + im[k] * wr;
        re[k] = re[j] - xr;
        im[k] = im[j] - xi;
        re[j] += xr;
        im[j] += xi;
        const tmp = wr * wr0 - wi * wi0;
        wi = wr * wi0 + wi * wr0;
        wr = tmp;
      }
    }
  }
}

const t = performance.now();
const logN = 16, n = 1 << logN, re = new Array(n), im = new Array(n);
let seed = 2024, spec = 0.0, maxErr = 0.0;
for (let i = 0; i < n; i++) {
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7FFFFFFF;
  re[i] = seed / 1073741824.0 - 1.0;
  seed = (Math.imul(seed, 1103515245) + 12345) & 0x7FFFFFFF;
  im[i] = seed / 1073741824.0 - 1.0;
}
const re0 = re.slice(), im0 = im.slice();
// cos and sin of 2 pi / 2^k, for k from 1
const cosT = new Array(logN), sinT = new Array(logN);
for (let i = 0; i < logN; i++) {
  cosT[i] = Math.cos(2.0 * Math.PI / 2 ** (i + 1));
  sinT[i] = Math.sin(2.0 * Math.PI / 2 ** (i + 1));
}
for (let rep = 1; rep <= 10; rep++) {
  fft(re, im, n, cosT, sinT, -1.0);
  if (rep === 1) for (let i = 0; i < n; i++) spec += Math.abs(re[i]) + Math.abs(im[i]);
  fft(re, im, n, cosT, sinT, 1.0);
  for (let i = 0; i < n; i++) {
    re[i] = re[i] / n;
    im[i] = im[i] / n;
  }
}
for (let i = 0; i < n; i++) maxErr = Math.max(maxErr, Math.max(Math.abs(re[i] - re0[i]), Math.abs(im[i] - im0[i])));
report("fft", t, Math.abs(spec / 15471706.35354992 - 1) < 0.000000001 && maxErr < 0.000000001);
