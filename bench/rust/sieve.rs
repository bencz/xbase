// bench/sieve.prg in Rust: the primes up to 2,000,000, 10 times.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n: usize = std::hint::black_box(2_000_000);
    let mut a = vec![false; n + 1];
    let mut ok = true;
    for _ in 0..10 {
        let mut count = 0;
        for i in 2..=n { a[i] = true; }
        a[1] = false;
        let mut i = 2;
        while i * i <= n {
            if a[i] {
                let mut j = i * i;
                while j <= n { a[j] = false; j += i; }
            }
            i += 1;
        }
        for i in 1..=n { if a[i] { count += 1; } }
        ok = ok && count == 148933;
    }
    println!("sieve {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
