// bench/numerical_integration.prg in Rust: Simpson's rule over 1,000,000 intervals for four integrals whose values
// are known (4 / (1 + x^2) and sqrt(x) on [0, 1], exp(-x^2) on [0, 1], 1 / x on [1, 2]), each function a closure
// called through &dyn Fn, 3 times.
use std::hint::black_box;
use std::time::Instant;

#[inline(never)]
fn simpson(f: &dyn Fn(f64) -> f64, a: f64, b: f64, n: i64) -> f64 {
    let h = (b - a) / n as f64;
    let mut s = f(a) + f(b);
    for i in 1..n {
        s += if i & 1 == 1 { 4.0 } else { 2.0 } * f(a + i as f64 * h);
    }
    s * h / 3.0
}

fn main() {
    let t = Instant::now();
    let n = black_box(1000000i64);
    let mut ok = true;
    for _ in 0..3 {
        let r1 = simpson(black_box(&|x: f64| 4.0 / (1.0 + x * x)), 0.0, 1.0, n);
        let r2 = simpson(black_box(&|x: f64| (-(x * x)).exp()), 0.0, 1.0, n);
        let r3 = simpson(black_box(&|x: f64| 1.0 / x), 1.0, 2.0, n);
        let r4 = simpson(black_box(&|x: f64| x.sqrt()), 0.0, 1.0, n);
        ok = ok && (r1 - 3.141592653589793).abs() < 1e-12 && (r2 - 0.746824132812427).abs() < 1e-12
            && (r3 - 0.6931471805599453).abs() < 1e-12 && (r4 - 2.0 / 3.0).abs() < 1e-9;
    }
    println!("numerical_integration {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
