// bench/substr_massivo.prg in Rust: 6,000,000 pieces of 1 to 20 bytes cut (each a new String) at spread positions of
// a 1,000,000-byte text made from a fixed seed, their first and last bytes read.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let mut seed: i64 = 5;
    let mut c = String::new();
    for _ in 0..1_000_000 {
        seed = seed * 16807 % 2147483647;
        c.push(char::from(33 + (seed % 90) as u8));
    }
    let c = black_box(c);
    let t = Instant::now();
    let mut n: i64 = 0;
    for i in 1..=6_000_000usize {
        let start = (i * 7919) % 999970;
        let s = black_box(c[start..start + i % 20 + 1].to_string());
        let b = s.as_bytes();
        n += i64::from(b[0]) + i64::from(b[b.len() - 1]) + b.len() as i64;
    }
    let ok = n == 994529563;
    println!("substr_massivo {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
