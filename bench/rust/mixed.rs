// bench/mixed.prg in Rust: integers and doubles in one expression, double to integer, comparisons across both.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let limit = std::hint::black_box(20_000_000i64);
    let (mut s, mut n) = (0.0f64, 0i64);
    for i in 1..=limit {
        let d = i as f64 + 3.14;
        s += ((d * 4.0) as i64 % 7) as f64;
        if d > (i + 3) as f64 { n += 1; }
        s += i as f64 / 4.0;
    }
    let ok = s == 50000062499998.0 && n == 20000000;
    println!("mixed {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
