// bench/sieve.prg in JavaScript: the primes up to 2,000,000, 10 times.
const { report } = require("./bench.js");
const t = performance.now();
const n = 2000000;
const a = new Uint8Array(n + 1);
let ok = true;
for (let rep = 1; rep <= 10; rep++) {
  let count = 0;
  for (let i = 2; i <= n; i++) a[i] = 1;
  a[1] = 0;
  for (let i = 2; i * i <= n; i++) if (a[i]) for (let j = i * i; j <= n; j += i) a[j] = 0;
  for (let i = 1; i <= n; i++) if (a[i]) count++;
  ok = ok && count === 148933;
}
report("sieve", t, ok);
