// bench/whetstone.prg in Rust: Whetstone's modules 2, 3, 4, 6, 7, 8, 9 and 11 scaled by a loop count of 5,000; the
// procedures are not inlined; the check is the sum of the values each module leaves. Arrays are 1-based as in the
// .prg (element 0 unused).
use std::hint::black_box;
use std::time::Instant;

const T: f64 = 0.499975;
const T1: f64 = 0.50025;
const T2: f64 = 2.0;

#[inline(never)]
fn pa(e: &mut [f64; 5]) {
    for _ in 0..6 {
        e[1] = (e[1] + e[2] + e[3] - e[4]) * T;
        e[2] = (e[1] + e[2] - e[3] + e[4]) * T;
        e[3] = (e[1] - e[2] + e[3] + e[4]) * T;
        e[4] = (-e[1] + e[2] + e[3] + e[4]) / T2;
    }
}

#[inline(never)]
fn p0(e1: &mut [f64; 5], j: usize, k: usize, l: usize) {
    e1[j] = e1[k];
    e1[k] = e1[l];
    e1[l] = e1[j];
}

#[inline(never)]
fn p3(x: f64, y: f64, z: &mut f64) {
    let mut x1 = x;
    let mut y1 = y;
    x1 = T * (x1 + y1);
    y1 = T * (x1 + y1);
    *z = (x1 + y1) / T2;
}

fn main() {
    let tm = Instant::now();
    let n_loop: i64 = black_box(5000);
    let mut e1 = [0.0, 1.0, -1.0, -1.0, -1.0];
    for _ in 0..12 * n_loop {
        e1[1] = (e1[1] + e1[2] + e1[3] - e1[4]) * T;
        e1[2] = (e1[1] + e1[2] - e1[3] + e1[4]) * T;
        e1[3] = (e1[1] - e1[2] + e1[3] + e1[4]) * T;
        e1[4] = (-e1[1] + e1[2] + e1[3] + e1[4]) * T;
    }
    let mut check = e1[1] + e1[2] + e1[3] + e1[4];
    for _ in 0..14 * n_loop {
        pa(&mut e1);
    }
    check += e1[1] + e1[2] + e1[3] + e1[4];
    let mut j: i64 = 1;
    for _ in 0..345 * n_loop {
        j = if j == 1 { 2 } else { 3 };
        j = if j > 2 { 0 } else { 1 };
        j = if j < 1 { 1 } else { 0 };
    }
    check += j as f64;
    j = 1;
    let mut k: i64 = 2;
    let mut l: i64 = 3;
    for _ in 0..210 * n_loop {
        j = j * (k - j) * (l - k);
        k = l * k - (l - j) * k;
        l = (l - k) * (k + j);
        e1[(l - 1) as usize] = (j + k + l) as f64;
        e1[(k - 1) as usize] = (j * k * l) as f64;
    }
    check += (j + k + l) as f64 + e1[1] + e1[2] + e1[3] + e1[4];
    let mut x = 0.5f64;
    let mut y = 0.5f64;
    for _ in 0..32 * n_loop {
        x = T * (T2 * x.sin() * x.cos() / ((x + y).cos() + (x - y).cos() - 1.0)).atan();
        y = T * (T2 * y.sin() * y.cos() / ((x + y).cos() + (x - y).cos() - 1.0)).atan();
    }
    check += x + y;
    let (x, y) = (1.0, 1.0);
    let mut z = 1.0;
    for _ in 0..899 * n_loop {
        p3(x, y, &mut z);
    }
    check += x + y + z;
    e1[1] = 1.0;
    e1[2] = 2.0;
    e1[3] = 3.0;
    for _ in 0..616 * n_loop {
        p0(&mut e1, 1, 2, 3);
    }
    check += e1[1] + e1[2] + e1[3] + e1[4];
    let mut x = 0.75f64;
    for _ in 0..93 * n_loop {
        x = (x.ln() / T1).exp().sqrt();
    }
    check += x;
    let ok = (check - 30.95612841002271).abs() < 0.000000001;
    println!("whetstone {} {}", tm.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
