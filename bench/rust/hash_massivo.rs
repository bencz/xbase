// bench/hash_massivo.prg in Rust: 1,000,000 numeric keys from a fixed-seed generator counted in one HashMap (about
// 632,000 distinct), then 1,500,000 lookups of keys drawn from twice the range, about two thirds of them missing.
use std::collections::HashMap;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let mut seed: i64 = 2024;
    let mut rand = |n: i64| { seed = seed * 16807 % 2147483647; seed % n };
    let mut h: HashMap<i64, i64> = HashMap::new();
    for _ in 0..1_000_000 {
        *h.entry(rand(1_000_000)).or_insert(0) += 1;
    }
    let (mut hits, mut sum) = (0i64, 0i64);
    for _ in 0..1_500_000 {
        if let Some(v) = h.get(&rand(2_000_000)) {
            hits += 1;
            sum += v;
        }
    }
    let ok = h.len() == 631967 && hits == 473472 && sum == 748930;
    println!("hash_massivo {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
