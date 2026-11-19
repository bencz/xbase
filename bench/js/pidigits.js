// bench/pidigits.prg in JavaScript: arbitrary precision in arrays: the first 1,500 digits of pi from Gibbons'
// unbounded spigot, its numbers kept as arrays of 30-bit limbs (the least significant first; the accumulator's sign
// apart), with multiplication by a small number, addition and subtraction of a multiple, comparison, and a quotient
// below 2^30 estimated from the leading limbs with doubles, then corrected. Checked: the first and the last 50 digits
// and their sum. A limb times a multiplier (below 2^23 here) stays below 2^53, so numbers hold every step exactly;
// the .prg's shift and mask of a possibly negative step (floor division by 2^30 and its remainder) are written as
// Math.floor and a subtraction, as JavaScript's bit operators work on 32 bits.
const { report } = require("./bench.js");
const RADIX = 1073741824;

const high = (x) => Math.floor(x / RADIX);
const low = (x) => x - Math.floor(x / RADIX) * RADIX;

// Drops the leading zero limbs (keeps one).
function normalize(a) {
  let n = a.length;
  while (n > 1 && a[n - 1] === 0) n--;
  if (n < a.length) a.length = n;
}

// a *= m
function mulSmall(a, m) {
  let carry = 0;
  for (let i = 0; i < a.length; i++) {
    const x = a[i] * m + carry;
    a[i] = low(x);
    carry = high(x);
  }
  while (carry > 0) {
    a.push(low(carry));
    carry = high(carry);
  }
  normalize(a);
}

// a += b * m
function addMul(a, b, m) {
  const nB = b.length;
  while (a.length < nB) a.push(0);
  let carry = 0, i;
  for (i = 0; i < nB; i++) {
    const x = a[i] + b[i] * m + carry;
    a[i] = low(x);
    carry = high(x);
  }
  while (carry > 0 && i < a.length) {
    const x = a[i] + carry;
    a[i] = low(x);
    carry = high(x);
    i++;
  }
  while (carry > 0) {
    a.push(low(carry));
    carry = high(carry);
  }
}

// a -= b * m, when b * m <= a
function subMul(a, b, m) {
  let borrow = 0, i;
  for (i = 0; i < b.length; i++) {
    const x = a[i] - b[i] * m - borrow;
    a[i] = low(x);
    borrow = -high(x);
  }
  while (borrow > 0) {
    const x = a[i] - borrow;
    a[i] = low(x);
    borrow = -high(x);
    i++;
  }
  normalize(a);
}

// a += b * m (m >= 0), neg[0] the sign of a, termNeg that of the term.
function addSigned(a, neg, b, m, termNeg) {
  if (neg[0] === termNeg) {
    addMul(a, b, m);
  } else {
    const tmp = b.slice();
    mulSmall(tmp, m);
    if (cmp(a, tmp) >= 0) {
      subMul(a, tmp, 1);
    } else {
      subMul(tmp, a, 1);
      a.length = tmp.length;
      for (let i = 0; i < tmp.length; i++) a[i] = tmp[i];
      neg[0] = termNeg;
    }
  }
  if (a.length === 1 && a[0] === 0) neg[0] = false;
}

function cmp(a, b) {
  if (a.length !== b.length) return a.length < b.length ? -1 : 1;
  for (let i = a.length - 1; i >= 0; i--) if (a[i] !== b[i]) return a[i] < b[i] ? -1 : 1;
  return 0;
}

// The limb at i (from 0) as a double, 0 past either end.
const top = (a, i) => (i >= 0 && i < a.length ? a[i] : 0.0);

// Int( x / d ) for a quotient below 2^30: estimated from the three leading limbs, then corrected.
function quotient(x, d) {
  const h = Math.max(x.length, d.length) - 1;
  const fx = (top(x, h) * RADIX + top(x, h - 1)) * RADIX + top(x, h - 2);
  const fd = (top(d, h) * RADIX + top(d, h - 1)) * RADIX + top(d, h - 2);
  let q = Math.trunc(fx / fd);
  const tmp = d.slice();
  mulSmall(tmp, q);
  while (cmp(tmp, x) > 0) {
    subMul(tmp, d, 1);
    q--;
  }
  addMul(tmp, d, 1);
  while (cmp(tmp, x) <= 0) {
    addMul(tmp, d, 1);
    q++;
  }
  return q;
}

const t = performance.now();
const n = 1500, numer = [1], accum = [0], denom = [1], neg = [false];
let digits = "", k = 0, sum = 0;
while (digits.length < n) {
  k++;
  addSigned(accum, neg, numer, 2, false);
  mulSmall(accum, 2 * k + 1);
  mulSmall(denom, 2 * k + 1);
  mulSmall(numer, k);
  if (neg[0] || cmp(numer, accum) > 0) continue;
  const x = accum.slice(), xNeg = [neg[0]];
  addSigned(x, xNeg, numer, 3, false);
  const d = quotient(x, denom);
  addSigned(x, xNeg, numer, 1, false);
  if (d !== quotient(x, denom)) continue;
  digits += String.fromCharCode(48 + d);
  addSigned(accum, neg, denom, d, true);
  mulSmall(accum, 10);
  mulSmall(numer, 10);
}
for (let i = 0; i < n; i++) sum += digits.charCodeAt(i) - 48;
const ok = digits.slice(0, 50) === "31415926535897932384626433832795028841971693993751" &&
  digits.slice(-50) === "55570674983850549458858692699569092721079750930295" && sum === 6775;
report("pidigits", t, ok);
