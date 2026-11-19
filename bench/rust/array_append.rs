// bench/array_append.prg in Rust: 5 vectors of 2,000,000 numbers built with push from empty, then summed.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let mut n_sum: i64 = 0;
    let mut a: Vec<i64> = Vec::new();
    for r in 1..=5i64 {
        a = Vec::new();
        for i in 1..=black_box(2_000_000i64) {
            a.push(i + r);
        }
        a = black_box(a);
        for x in &a {
            n_sum += x;
        }
    }
    let ok = n_sum == 10000035000000 && a.len() == 2000000;
    println!("array_append {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
