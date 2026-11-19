// bench/tokenize_split.prg in Rust: 200,000 comma-separated lines of 6 fields split into vectors of new strings
// (str::split), the numeric fields parsed; 2 rounds.
use std::time::Instant;

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = (*seed * 16807) % 2147483647;
    *seed % n
}

fn val(s: &str) -> i64 { s.parse().unwrap_or(0) }

fn main() {
    let mut seed: i64 = 64;
    let mut lines: Vec<String> = Vec::with_capacity(200000);
    for i in 1..=200000i64 {
        let a = rand(&mut seed, 100000);
        let b = rand(&mut seed, 1000);
        let z = rand(&mut seed, 12) as usize;
        lines.push(format!("{},name{},{},,{},end", a, i, b, "z".repeat(z)));
    }
    let lines = std::hint::black_box(lines);
    let t = Instant::now();
    let mut n: i64 = 0;
    for _ in 0..2 {
        for c in &lines {
            let a: Vec<String> = c.split(',').map(String::from).collect();
            n += val(&a[0]) + val(&a[2]) + a.len() as i64 + a[4].len() as i64;
        }
    }
    let ok = n == 20190509958;
    println!("tokenize_split {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
