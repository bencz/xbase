// bench/higher_order_chain.prg in Rust: chains of iterator adapters over a Vec of 500,000 numbers, each step fed by
// the one before: filter, map, skip, take_while and sum; map with the position, filter, take and count; distinct
// by a key (a HashSet of the keys seen) and count; 5 rounds.
use std::collections::HashSet;
use std::time::Instant;

fn main() {
    let a: Vec<i64> = (1..=500000i64).map(|i| (i * 37) % 1000).collect();
    let a = std::hint::black_box(a);
    let t = Instant::now();
    let mut n: i64 = 0;
    for r in 1..=5i64 {
        n += a.iter().filter(|&&x| x % 3 != 0).map(|&x| x * r).skip(10).take_while(|&x| x < 4900).sum::<i64>();
        n += a
            .iter()
            .enumerate()
            .map(|(p, &x)| x + (p as i64 + 1) % 7)
            .filter(|&x| x % 2 == 0)
            .take((50000 + r) as usize)
            .count() as i64;
        let mut seen = HashSet::new();
        n += a.iter().filter(|&&x| seen.insert(x % (100 * r))).count() as i64;
    }
    let ok = n == 1663633050;
    println!("higher_order_chain {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
