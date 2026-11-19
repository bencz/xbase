// bench/array_preallocate.prg in Rust: 25 vectors of 2,000,000 numbers made at their size, filled by index, then
// summed by index.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n = black_box(2_000_000usize);
    let mut sum: i64 = 0;
    let mut a: Vec<i64> = Vec::new();
    for r in 1..=25i64 {
        a = black_box(vec![0i64; n]);
        for i in 0..n {
            a[i] = i as i64 + 1 + r;
        }
        let a = black_box(&a);
        for i in 0..n {
            sum += a[i];
        }
    }
    let ok = sum == 50000675000000 && a.len() == n;
    println!("array_preallocate {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
