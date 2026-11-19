// bench/hash_resize.prg in Rust: a HashMap that grows and shrinks: 20 rounds of 100,000 keys added to an empty map
// and removed again, the even ones first, then the odd ones (shrink_to_fit after each removal, as the xBase hash's
// store shrinks as it goes).
use std::collections::HashMap;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let mut ok = true;
    let mut sum: i64 = 0;
    for r in 1..=20i64 {
        let mut h: HashMap<i64, i64> = HashMap::new();
        for i in 1..=100000i64 {
            h.insert(i * 7 + r, i);
        }
        ok = ok && h.len() == 100000;
        for i in (2..=100000i64).step_by(2) {
            sum += h[&(i * 7 + r)];
            h.remove(&(i * 7 + r));
            h.shrink_to_fit();
        }
        ok = ok && h.len() == 50000;
        for i in (1..=100000i64).step_by(2) {
            sum += h[&(i * 7 + r)];
            h.remove(&(i * 7 + r));
            h.shrink_to_fit();
        }
        ok = ok && h.is_empty();
    }
    ok = ok && sum == 100001000000;
    println!("hash_resize {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
