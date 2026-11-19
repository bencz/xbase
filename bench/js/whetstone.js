// bench/whetstone.prg in JavaScript: Whetstone (Curnow and Wichmann's synthetic benchmark, its modules scaled by a
// loop count of 5,000): array elements, an array as a parameter, conditional jumps, integer arithmetic, procedure calls
// with a parameter by reference (a one-element array here), array references, and
// Math.sqrt( Math.exp( Math.log( x ) / t ) ). Modules 1, 7 and 10 are left out, as in the .prg. The check is the sum
// of the values each module leaves. e1 is numbered from 1, as in the .prg (element 0 unused).
const { report } = require("./bench.js");
const t = 0.499975, t1 = 0.50025, t2 = 2.0;
let e1, j, k, l;

function pa(e) {
  let n = 0;
  while (n < 6) {
    e[1] = (e[1] + e[2] + e[3] - e[4]) * t;
    e[2] = (e[1] + e[2] - e[3] + e[4]) * t;
    e[3] = (e[1] - e[2] + e[3] + e[4]) * t;
    e[4] = (-e[1] + e[2] + e[3] + e[4]) / t2;
    n++;
  }
}

function p0() {
  e1[j] = e1[k];
  e1[k] = e1[l];
  e1[l] = e1[j];
}

function p3(x, y, z) {
  let x1 = x, y1 = y;
  x1 = t * (x1 + y1);
  y1 = t * (x1 + y1);
  z[0] = (x1 + y1) / t2;
}

const tm = performance.now();
e1 = [0, 1.0, -1.0, -1.0, -1.0];
const nLoop = 5000;
for (let i = 1; i <= 12 * nLoop; i++) {
  e1[1] = (e1[1] + e1[2] + e1[3] - e1[4]) * t;
  e1[2] = (e1[1] + e1[2] - e1[3] + e1[4]) * t;
  e1[3] = (e1[1] - e1[2] + e1[3] + e1[4]) * t;
  e1[4] = (-e1[1] + e1[2] + e1[3] + e1[4]) * t;
}
let check = e1[1] + e1[2] + e1[3] + e1[4];
for (let i = 1; i <= 14 * nLoop; i++) pa(e1);
check += e1[1] + e1[2] + e1[3] + e1[4];
j = 1;
for (let i = 1; i <= 345 * nLoop; i++) {
  j = j === 1 ? 2 : 3;
  j = j > 2 ? 0 : 1;
  j = j < 1 ? 1 : 0;
}
check += j;
j = 1;
k = 2;
l = 3;
for (let i = 1; i <= 210 * nLoop; i++) {
  j = j * (k - j) * (l - k);
  k = l * k - (l - j) * k;
  l = (l - k) * (k + j);
  e1[l - 1] = j + k + l;
  e1[k - 1] = j * k * l;
}
check += j + k + l + e1[1] + e1[2] + e1[3] + e1[4];
let x = 1.0, y = 1.0;
const z = [1.0];
for (let i = 1; i <= 899 * nLoop; i++) p3(x, y, z);
check += x + y + z[0];
j = 1;
k = 2;
l = 3;
e1[1] = 1.0;
e1[2] = 2.0;
e1[3] = 3.0;
for (let i = 1; i <= 616 * nLoop; i++) p0();
check += e1[1] + e1[2] + e1[3] + e1[4];
let w = 0.75;
for (let i = 1; i <= 93 * nLoop; i++) w = Math.sqrt(Math.exp(Math.log(w) / t1));
check += w;
report("whetstone", tm, Math.abs(check - 30.95582672351981) < 0.000000001);
