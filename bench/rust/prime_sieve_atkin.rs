// bench/prime_sieve_atkin.prg in Rust: the sieve of Atkin, the primes up to 2,000,000 from the three quadratic forms
// taken modulo 12 (toggling a vector of bools), then the multiples of the squares of primes struck out; 12 times.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let limit: i64 = std::hint::black_box(2000000);
    let mut a = vec![false; limit as usize + 1];
    let mut ok = true;
    for _ in 0..12 {
        a.fill(false);
        let mut x: i64 = 1;
        while x * x <= limit {
            let mut y: i64 = 1;
            while y * y <= limit {
                let n = 4 * x * x + y * y;
                if n <= limit {
                    let r = n % 12;
                    if r == 1 || r == 5 {
                        a[n as usize] = !a[n as usize];
                    }
                }
                let n = 3 * x * x + y * y;
                if n <= limit && n % 12 == 7 {
                    a[n as usize] = !a[n as usize];
                }
                if x > y {
                    let n = 3 * x * x - y * y;
                    if n <= limit && n % 12 == 11 {
                        a[n as usize] = !a[n as usize];
                    }
                }
                y += 1;
            }
            x += 1;
        }
        let mut i: i64 = 5;
        while i * i <= limit {
            if a[i as usize] {
                let k = i * i;
                for j in (k..=limit).step_by(k as usize) {
                    a[j as usize] = false;
                }
            }
            i += 1;
        }
        let mut count = 2;
        for i in 5..=limit as usize {
            if a[i] {
                count += 1;
            }
        }
        ok = ok && count == 148933;
    }
    println!("prime_sieve_atkin {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
