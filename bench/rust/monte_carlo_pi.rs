// bench/monte_carlo_pi.prg in Rust: 50,000,000 points of the unit square from a 31-bit linear congruential generator,
// the count of those inside the quarter circle.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n: i64 = std::hint::black_box(50_000_000);
    let mut seed: i64 = 42;
    let mut inside: i64 = 0;
    for _ in 1..=n {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        let x = seed as f64 / 2147483648.0;
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        let y = seed as f64 / 2147483648.0;
        if x * x + y * y <= 1.0 {
            inside += 1;
        }
    }
    let est = 4.0 * inside as f64 / n as f64;
    let ok = inside == 39267523 && (est - 3.14159).abs() < 0.01;
    println!("monte_carlo_pi {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
