// bench/binary_search.prg in Rust: binary search written out: 1,000,000 keys from a fixed-seed generator looked for
// in a sorted Vec of 1,000,000 numbers (every third number), about a third of them found.
use std::time::Instant;

fn main() {
    let a: Vec<i64> = std::hint::black_box((1..=1000000i64).map(|i| i * 3).collect());
    let mut seed: i64 = 4711;
    let t = Instant::now();
    let mut found: i64 = 0;
    let mut pos_sum: i64 = 0;
    for _ in 0..1000000 {
        seed = (seed * 16807) % 2147483647;
        let k = seed % 3000010;
        let mut lo: i64 = 1;
        let mut hi: i64 = a.len() as i64;
        while lo <= hi {
            let mid = (lo + hi) / 2;
            let v = a[(mid - 1) as usize];
            if v < k {
                lo = mid + 1;
            } else if v > k {
                hi = mid - 1;
            } else {
                found += 1;
                pos_sum += mid;
                break;
            }
        }
    }
    let ok = found == 333181 && pos_sum == 166512579113;
    println!("binary_search {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
