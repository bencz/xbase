// bench/linpack.prg in JavaScript: LINPACK: a dense 300 x 300 system (the original's matrix generator) solved by LU
// factorization with partial pivoting (DGEFA, DGESL, through DAXPY, IDAMAX and DSCAL on one column-major array), 5
// times; the check is the solution, all ones, and the residual of A x - b. Arrays are numbered from 1, as in the .prg
// (element 0 unused).
const { report } = require("./bench.js");

// The original's test matrix: entries from a congruential sequence in [-2, 2), b the sums of the rows.
function matGen(a, b, n) {
  let init = 1325;
  for (let j = 1; j <= n; j++) {
    for (let i = 1; i <= n; i++) {
      init = (3125 * init) % 65536;
      a[(j - 1) * n + i] = (init - 32768.0) / 16384.0;
    }
  }
  b.fill(0.0, 1);
  for (let j = 1; j <= n; j++) for (let i = 1; i <= n; i++) b[i] += a[(j - 1) * n + i];
}

// LU factorization of the column-major n x n matrix a in place, the pivot rows in ipvt.
function dgefa(a, n, ipvt) {
  for (let k = 1; k <= n - 1; k++) {
    const col = (k - 1) * n, l = idamax(n - k + 1, a, col + k) + k - 1;
    ipvt[k] = l;
    if (a[col + l] !== 0.0) {
      let t;
      if (l !== k) {
        t = a[col + l];
        a[col + l] = a[col + k];
        a[col + k] = t;
      }
      t = -1.0 / a[col + k];
      dscal(n - k, t, a, col + k + 1);
      for (let j = k + 1; j <= n; j++) {
        t = a[(j - 1) * n + l];
        if (l !== k) {
          a[(j - 1) * n + l] = a[(j - 1) * n + k];
          a[(j - 1) * n + k] = t;
        }
        daxpy(n - k, t, a, col + k + 1, a, (j - 1) * n + k + 1);
      }
    }
  }
  ipvt[n] = n;
}

// Solves a x = b with the factors of dgefa; b becomes x.
function dgesl(a, n, ipvt, b) {
  for (let k = 1; k <= n - 1; k++) {
    const l = ipvt[k], t = b[l];
    if (l !== k) {
      b[l] = b[k];
      b[k] = t;
    }
    daxpy(n - k, t, a, (k - 1) * n + k + 1, b, k + 1);
  }
  for (let k = n; k >= 1; k--) {
    b[k] = b[k] / a[(k - 1) * n + k];
    daxpy(k - 1, -b[k], a, (k - 1) * n + 1, b, 1);
  }
}

// dy[ y .. y + n - 1 ] += da * dx[ x .. x + n - 1 ]
function daxpy(n, da, dx, x, dy, y) {
  if (n > 0 && da !== 0.0) for (let i = 0; i <= n - 1; i++) dy[y + i] += da * dx[x + i];
}

// dx[ x .. x + n - 1 ] *= da
function dscal(n, da, dx, x) {
  for (let i = x; i <= x + n - 1; i++) dx[i] = da * dx[i];
}

// The 1-based position of the largest magnitude in dx[ x .. x + n - 1 ] (the first of equal ones).
function idamax(n, dx, x) {
  let imax = 1, dmax = Math.abs(dx[x]);
  for (let i = 2; i <= n; i++) {
    const v = Math.abs(dx[x + i - 1]);
    if (v > dmax) {
      imax = i;
      dmax = v;
    }
  }
  return imax;
}

const t = performance.now();
const n = 300;
let ok = true;
for (let rep = 1; rep <= 5; rep++) {
  const a = new Array(n * n + 1), b = new Array(n + 1), ipvt = new Array(n + 1);
  matGen(a, b, n);
  const x = b.slice();
  dgefa(a, n, ipvt);
  dgesl(a, n, ipvt, x);
  // the factorization overwrote a: a new copy gives the residual
  matGen(a, b, n);
  let maxErr = 0.0, resid = 0.0;
  for (let i = 1; i <= n; i++) {
    maxErr = Math.max(maxErr, Math.abs(x[i] - 1.0));
    let s = -b[i];
    for (let j = 1; j <= n; j++) s += a[(j - 1) * n + i] * x[j];
    resid = Math.max(resid, Math.abs(s));
  }
  ok = ok && maxErr < 0.000000001 && resid < 0.000000001;
}
report("linpack", t, ok);
