// bench/gaussian_elimination.prg in Rust: doubles in nested vectors: a 300 x 300 system of random doubles (a fixed
// seed) whose solution is all ones, solved by Gaussian elimination with partial pivoting (rows swapped as vectors)
// and back substitution, 6 times.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n = black_box(300usize);
    let mut ok = true;
    for _ in 0..6 {
        let mut seed: i64 = 42;
        let mut a: Vec<Vec<f64>> = vec![vec![0.0; n + 1]; n];
        for row in a.iter_mut() {
            let mut s = 0.0;
            for v in row[..n].iter_mut() {
                seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
                *v = seed as f64 / 2147483648.0 - 0.5;
                s += *v;
            }
            row[n] = s;
        }
        for k in 0..n {
            let mut p = k;
            for i in k + 1..n {
                if a[i][k].abs() > a[p][k].abs() {
                    p = i;
                }
            }
            a.swap(p, k);
            let (top, bottom) = a.split_at_mut(k + 1);
            let piv = &top[k];
            for row in bottom.iter_mut() {
                let f = row[k] / piv[k];
                for j in k..=n {
                    row[j] -= f * piv[j];
                }
            }
        }
        let mut x = vec![0.0f64; n];
        let mut err = 0.0f64;
        for i in (0..n).rev() {
            let row = &a[i];
            let mut s = row[n];
            for j in i + 1..n {
                s -= row[j] * x[j];
            }
            x[i] = s / row[i];
            err = err.max((x[i] - 1.0).abs());
        }
        ok = ok && err < 0.000000001;
    }
    println!("gaussian_elimination {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
