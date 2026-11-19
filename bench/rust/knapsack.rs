// bench/knapsack.prg in Rust: 0/1 knapsack by dynamic programming over one vector of capacities, 200 items (weights
// and values 1 .. 1000 from a linear congruential generator) into capacities up to 10,000, 10 times with different
// seeds.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n = 200usize;
    let cap = black_box(10000usize);
    let mut best = vec![0i64; cap + 1];
    let mut w = vec![0usize; n];
    let mut v = vec![0i64; n];
    let mut total: i64 = 0;
    for rep in 1..=10i64 {
        let mut seed = rep;
        for i in 0..n {
            seed = seed * 16807 % 2147483647;
            w[i] = ((seed / 65536) % 1000 + 1) as usize;
            seed = seed * 16807 % 2147483647;
            v[i] = (seed / 65536) % 1000 + 1;
        }
        best.fill(0);
        for i in 0..n {
            let wi = w[i];
            let vi = v[i];
            for c in (wi..=cap).rev() {
                let x = best[c - wi] + vi;
                if x > best[c] {
                    best[c] = x;
                }
            }
        }
        total += best[cap];
    }
    println!("knapsack {} {}", t.elapsed().as_millis(), if total == 366516 { "ok" } else { "WRONG" });
}
