// bench/random_massivo.prg in JavaScript: massive random-number generation, 30,000,000 rounds of four generators in
// integer arithmetic (a 31-bit linear congruential generator, a 64-bit xorshift with masks for the logical right
// shift, Marsaglia's two 16-bit multiply-with-carry streams and a 32-bit xorshift), their sums, a histogram of 256
// buckets of their top bytes and the sum of the congruential numbers scaled to [0, 1). The 64-bit xorshift and the
// four sums pass 2^53: they are BigInt (the xorshift wrapped to 64 signed bits, as xBase integers); the congruential
// generator's product passes 2^53 too, and Math.imul keeps its low 32 bits exact (only the low 31 are kept). The
// histogram keeps the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");
const t = performance.now();
const n = 30000000, hist = new Array(257).fill(0);
let lcg = 12345, xs64 = 88172645463325252n, z = 362436069, w = 521288629, xs32 = 2463534242;
let s1 = 0n, s2 = 0n, s3 = 0n, s4 = 0n, fsum = 0.0, check = 0, r;
for (let i = 1; i <= n; i++) {
  lcg = (Math.imul(lcg, 1103515245) + 12345) & 0x7fffffff;
  s1 += BigInt(lcg);
  hist[(lcg >> 23) + 1]++;
  fsum += lcg / 2147483648.0;

  xs64 = BigInt.asIntN(64, xs64 ^ (xs64 << 13n));
  xs64 = xs64 ^ ((xs64 >> 7n) & 0x01ffffffffffffffn);
  xs64 = BigInt.asIntN(64, xs64 ^ (xs64 << 17n));
  const r64 = (xs64 >> 32n) & 0xffffffffn;
  s2 += r64;
  hist[Number(r64 >> 24n) + 1]++;

  // z and w stay below 2^32 and z * 65536 + w below 2^53: Number arithmetic is exact, the mask taken as unsigned.
  z = 36969 * (z & 0xffff) + Math.floor(z / 65536);
  w = 18000 * (w & 0xffff) + Math.floor(w / 65536);
  r = (z * 65536 + w) % 4294967296;
  s3 += BigInt(r);
  hist[Math.floor(r / 16777216) + 1]++;

  xs32 = (xs32 ^ (xs32 << 13)) >>> 0;
  xs32 = (xs32 ^ (xs32 >>> 17)) >>> 0;
  xs32 = (xs32 ^ (xs32 << 5)) >>> 0;
  s4 += BigInt(xs32);
  hist[(xs32 >>> 24) + 1]++;
}
for (let i = 1; i <= 256; i++) check += hist[i] * i;
const ok = s1 === 32217797789519680n && s2 === 64428429046975605n && s3 === 64423232821249815n &&
  s4 === 64428119329011698n && check === 15421035017 && Math.abs(fsum - 15002581.187856009) < 0.000001;
report("random_massivo", t, ok);
