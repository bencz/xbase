// bench/substr_massivo.prg in JavaScript: 6,000,000 pieces of 1 to 20 bytes cut with substr() at spread positions of
// a 1,000,000-byte text, their first and last bytes read.
const { report } = require("./bench.js");

let seed = 5;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

let c = "";
for (let i = 1; i <= 1000000; i++) c += String.fromCharCode(33 + rand(90));
const t = performance.now();
let n = 0;
for (let i = 1; i <= 6000000; i++) {
  const s = c.substr((i * 7919) % 999970, (i % 20) + 1);
  n += s.charCodeAt(0) + s.charCodeAt(s.length - 1) + s.length;
}
report("substr_massivo", t, n === 994529563);
