// bench/spectral_norm.prg in Rust: the spectral norm of the infinite matrix A(i,j) = 1 / ((i+j)(i+j+1)/2 + i + 1) cut
// to 1,200 x 1,200: 10 rounds of the power method on A transposed times A, each element of A from a function call
// (not inlined).
use std::hint::black_box;
use std::time::Instant;

// The element of A at the 0-based row i and column j.
#[inline(never)]
fn eval_a(i: i64, j: i64) -> f64 {
    1.0 / ((i + j) * (i + j + 1) / 2 + i + 1) as f64
}

// v := A * u
fn mul_av(u: &[f64], v: &mut [f64]) {
    for (i, vi) in v.iter_mut().enumerate() {
        let mut s = 0.0;
        for (j, uj) in u.iter().enumerate() {
            s += eval_a(i as i64, j as i64) * uj;
        }
        *vi = s;
    }
}

// v := A transposed * u
fn mul_atv(u: &[f64], v: &mut [f64]) {
    for (i, vi) in v.iter_mut().enumerate() {
        let mut s = 0.0;
        for (j, uj) in u.iter().enumerate() {
            s += eval_a(j as i64, i as i64) * uj;
        }
        *vi = s;
    }
}

// v := A transposed * A * u, through w
fn mul_atav(u: &[f64], v: &mut [f64], w: &mut [f64]) {
    mul_av(u, w);
    mul_atv(w, v);
}

fn main() {
    let t = Instant::now();
    let n: usize = black_box(1200);
    let mut u = vec![1.0f64; n];
    let mut v = vec![0.0f64; n];
    let mut w = vec![0.0f64; n];
    for _ in 0..10 {
        mul_atav(&u, &mut v, &mut w);
        mul_atav(&v, &mut u, &mut w);
    }
    let (mut vbv, mut vv) = (0.0f64, 0.0f64);
    for i in 0..n {
        vbv += u[i] * v[i];
        vv += v[i] * v[i];
    }
    let r = (vbv / vv).sqrt();
    let ok = (r - 1.2742241501).abs() < 0.000000001;
    println!("spectral_norm {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
