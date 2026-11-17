// bench/asort.prg in JavaScript: the library's sort, 300,000 numbers ascending and 100,000 with a descending
// comparison, 5 times.
const { report } = require("./bench.js");

function sortBoth() {
  const a = new Array(300000), b = new Array(100000);
  let x = 777, sum = 0;
  for (let i = 0; i < a.length; i++) {
    x = (x * 16807) % 2147483647;
    a[i] = x % 1000000;
  }
  x = 4242;
  for (let i = 0; i < b.length; i++) {
    x = (x * 16807) % 2147483647;
    b[i] = x % 1000000;
  }
  a.sort((p, q) => p - q);
  b.sort((p, q) => q - p);
  for (let i = 0; i < a.length; i += 1000) sum += a[i];
  return a[0] === 8 && a[149999] === 500673 && a[299999] === 999998 && sum === 149589926 && b[0] === 999994 &&
    b[49999] === 501282 && b[99999] === 8;
}

const t = performance.now();
let ok = true;
for (let rep = 1; rep <= 5; rep++) ok = ok && sortBoth();
report("asort", t, ok);
