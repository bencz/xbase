// bench/prime_sieve_atkin.prg in JavaScript: the sieve of Atkin, the primes up to 2,000,000 from the three quadratic
// forms 4x^2+y^2, 3x^2+y^2 and 3x^2-y^2 taken modulo 12 (toggling an array of booleans), then the multiples of the
// squares of primes struck out; 12 times. The array keeps the .prg's 1-based indexes (element 0 unused).
const { report } = require("./bench.js");
const t = performance.now();
const limit = 2000000, a = new Array(limit + 1);
let ok = true;
for (let rep = 1; rep <= 12; rep++) {
  a.fill(false);
  for (let x = 1; x * x <= limit; x++) {
    for (let y = 1; y * y <= limit; y++) {
      let n = 4 * x * x + y * y;
      if (n <= limit) {
        const r = n % 12;
        if (r === 1 || r === 5) a[n] = !a[n];
      }
      n = 3 * x * x + y * y;
      if (n <= limit && n % 12 === 7) a[n] = !a[n];
      if (x > y) {
        n = 3 * x * x - y * y;
        if (n <= limit && n % 12 === 11) a[n] = !a[n];
      }
    }
  }
  for (let i = 5; i * i <= limit; i++) {
    if (a[i]) {
      const k = i * i;
      for (let j = k; j <= limit; j += k) a[j] = false;
    }
  }
  let count = 2;
  for (let i = 5; i <= limit; i++) if (a[i]) count++;
  ok = ok && count === 148933;
}
report("prime_sieve_atkin", t, ok);
