// bench/codeblock_callback.prg in JavaScript: functions that take a closure: Simpson's rule over 3 functions
// (1,000,000 intervals each), and an insertion sort of 3,000 numbers that compares through a closure, 10 times.
const { report } = require("./bench.js");

let seed = 1;
function rand(n) {
  seed = (seed * 16807) % 2147483647;
  return seed % n;
}

function simpson(f, a, b, n) {
  const h = (b - a) / n;
  let sum = f(a) + f(b);
  for (let i = 1; i <= n - 1; i++) sum += f(a + i * h) * (i % 2 === 1 ? 4 : 2);
  return (sum * h) / 3;
}
function insertionSort(a, less) {
  for (let i = 1; i < a.length; i++) {
    const x = a[i];
    let j = i - 1;
    while (j >= 0 && less(x, a[j])) {
      a[j + 1] = a[j];
      j--;
    }
    a[j + 1] = x;
  }
}

const data = new Array(3000);
for (let i = 0; i < data.length; i++) data[i] = rand(100000);
const t = performance.now();
let integral = 0, check = 0;
integral += simpson((x) => x * x, 0, 3, 1000000);
integral += simpson((x) => 1 / (1 + x * x), 0, 1, 1000000);
integral += simpson((x) => x * x * x - 2 * x, -1, 2, 1000000);
for (let r = 1; r <= 10; r++) {
  const a = data.slice();
  insertionSort(a, r % 2 === 0 ? (x, y) => x < y : (x, y) => x > y);
  check += a[0] + a[1499] * 2 + a[2999] * 3;
}
report("codeblock_callback", t, Math.abs(integral - 10.535398163397) < 0.000000001 && check === 2991620);
