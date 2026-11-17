// bench/loops.prg in Rust: the sum of the first 500,000,000 odd numbers.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let limit: i64 = std::hint::black_box(500_000_000);
    let mut n: i64 = 0;
    let mut i: i64 = 1;
    while i <= limit {
        n = n + i * 2 - 1;
        i += 1;
    }
    println!("loops {} {}", t.elapsed().as_millis(), if n == 250_000_000_000_000_000 { "ok" } else { "WRONG" });
}
