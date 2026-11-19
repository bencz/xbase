// bench/array_growth.prg in JavaScript: 300,000 arrays grown with push() to 1 to 64 elements, enlarged by setting
// their length to twice that, cut to half, an element inserted in front and removed again with copyWithin() (as
// AIns() and ADel(), which keep the length); one in 100 kept to the end.
const { report } = require("./bench.js");

let seed = 3;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

const t = performance.now();
const kept = [];
let n = 0;
for (let i = 1; i <= 300000; i++) {
  const a = [];
  const size = rand(64) + 1;
  for (let j = 1; j <= size; j++) a.push(j * 2);
  a.length = size * 2;
  a[size * 2 - 1] = i % 1000;
  n += a[size * 2 - 1];
  a.length = Math.floor((size + 1) / 2);
  a.copyWithin(1, 0, a.length - 1); // AIns( a, 1 ), then the new first element set
  a[0] = size;
  a.copyWithin(0, 1); // ADel( a, 1 ), then the freed last element set
  a[a.length - 1] = i % 7;
  n += a.length + a[a.length - 1] + (a.length > 1 ? a[0] : 0);
  if (i % 100 === 0) kept.push(a);
}
for (const a of kept) n += a[0];
report("array_growth", t, n === 156283982 && kept.length === 3000);
