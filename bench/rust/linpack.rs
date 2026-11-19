// bench/linpack.prg in Rust: LINPACK: a dense 300 x 300 system (the original's matrix generator) solved by LU
// factorization with partial pivoting (dgefa, dgesl, through daxpy, idamax and dscal on one column-major vector),
// 5 times; the check is the solution, all ones, and the residual of A x - b.
use std::hint::black_box;
use std::time::Instant;

// The original's test matrix: entries from a congruential sequence in [-2, 2), b the sums of the rows.
fn matgen(a: &mut [f64], b: &mut [f64], n: usize) {
    let mut init: i64 = 1325;
    for j in 0..n {
        for i in 0..n {
            init = (3125 * init) % 65536;
            a[j * n + i] = (init as f64 - 32768.0) / 16384.0;
        }
    }
    b.fill(0.0);
    for j in 0..n {
        for i in 0..n {
            b[i] += a[j * n + i];
        }
    }
}

// dy[0 .. n] += da * dx[0 .. n]
fn daxpy(n: usize, da: f64, dx: &[f64], dy: &mut [f64]) {
    if n > 0 && da != 0.0 {
        for i in 0..n {
            dy[i] += da * dx[i];
        }
    }
}

// dx[0 .. n] *= da
fn dscal(n: usize, da: f64, dx: &mut [f64]) {
    for v in dx[..n].iter_mut() {
        *v *= da;
    }
}

// The 0-based position of the largest magnitude in dx[0 .. n] (the first of equal ones).
fn idamax(n: usize, dx: &[f64]) -> usize {
    let (mut imax, mut dmax) = (0, dx[0].abs());
    for i in 1..n {
        let v = dx[i].abs();
        if v > dmax {
            imax = i;
            dmax = v;
        }
    }
    imax
}

// LU factorization of the column-major n x n matrix a in place, the pivot rows in ipvt.
fn dgefa(a: &mut [f64], n: usize, ipvt: &mut [usize]) {
    for k in 0..n - 1 {
        let col = k * n;
        let l = idamax(n - k, &a[col + k..]) + k;
        ipvt[k] = l;
        if a[col + l] != 0.0 {
            if l != k {
                a.swap(col + l, col + k);
            }
            let t = -1.0 / a[col + k];
            dscal(n - k - 1, t, &mut a[col + k + 1..]);
            for j in k + 1..n {
                let cj = j * n;
                let t = a[cj + l];
                if l != k {
                    a[cj + l] = a[cj + k];
                    a[cj + k] = t;
                }
                let (left, right) = a.split_at_mut(cj);
                daxpy(n - k - 1, t, &left[col + k + 1..], &mut right[k + 1..]);
            }
        }
    }
    ipvt[n - 1] = n - 1;
}

// Solves a x = b with the factors of dgefa; b becomes x.
fn dgesl(a: &[f64], n: usize, ipvt: &[usize], b: &mut [f64]) {
    for k in 0..n - 1 {
        let l = ipvt[k];
        let t = b[l];
        if l != k {
            b[l] = b[k];
            b[k] = t;
        }
        daxpy(n - k - 1, t, &a[k * n + k + 1..], &mut b[k + 1..]);
    }
    for k in (0..n).rev() {
        b[k] /= a[k * n + k];
        let t = -b[k];
        daxpy(k, t, &a[k * n..], b);
    }
}

fn main() {
    let t = Instant::now();
    let n = black_box(300usize);
    let mut ok = true;
    for _ in 0..5 {
        let mut a = vec![0.0f64; n * n];
        let mut b = vec![0.0f64; n];
        let mut ipvt = vec![0usize; n];
        matgen(&mut a, &mut b, n);
        let mut x = b.clone();
        dgefa(&mut a, n, &mut ipvt);
        dgesl(&a, n, &ipvt, &mut x);
        // the factorization overwrote a: a new copy gives the residual
        matgen(&mut a, &mut b, n);
        let (mut max_err, mut resid) = (0.0f64, 0.0f64);
        for i in 0..n {
            max_err = max_err.max((x[i] - 1.0).abs());
            let mut s = -b[i];
            for j in 0..n {
                s += a[j * n + i] * x[j];
            }
            resid = resid.max(s.abs());
        }
        ok = ok && max_err < 0.000000001 && resid < 0.000000001;
    }
    println!("linpack {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
