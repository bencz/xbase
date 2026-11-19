// bench/black_scholes.prg in Rust: Black-Scholes prices of 500,000 European options (a call and a put each), their
// inputs from a linear congruential generator, the sum of the prices.
use std::time::Instant;

// The cumulative normal distribution (the polynomial approximation of Abramowitz and Stegun, 26.2.17).
fn cnd(d: f64) -> f64 {
    let k = 1.0 / (1.0 + 0.2316419 * d.abs());
    let w = 1.0
        - 0.3989422804014327 * (-0.5 * d * d).exp()
            * k
            * (0.319381530 + k * (-0.356563782 + k * (1.781477937 + k * (-1.821255978 + k * 1.330274429))));
    if d < 0.0 { 1.0 - w } else { w }
}

fn main() {
    let t = Instant::now();
    let n: i64 = std::hint::black_box(500000);
    let mut seed: i64 = 12345;
    let (r, v) = (0.02f64, 0.30f64);
    let (mut sum_call, mut sum_put) = (0.0f64, 0.0f64);
    for _ in 1..=n {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        let s = 5.0 + 25.0 * seed as f64 / 2147483648.0;
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        let x = 1.0 + 99.0 * seed as f64 / 2147483648.0;
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        let tm = 0.25 + 9.75 * seed as f64 / 2147483648.0;
        let vst = v * tm.sqrt();
        let d1 = ((s / x).ln() + (r + v * v * 0.5) * tm) / vst;
        let d2 = d1 - vst;
        let disc = x * (-r * tm).exp();
        sum_call += s * cnd(d1) - disc * cnd(d2);
        sum_put += disc * cnd(-d2) - s * cnd(-d1);
    }
    let ok = (sum_call / 1494483.92385431 - 1.0).abs() < 0.000000001
        && (sum_put / 15576648.71605097 - 1.0).abs() < 0.000000001;
    println!("black_scholes {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
