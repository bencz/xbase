// bench/levenshtein.prg in Rust: the Levenshtein distance of 10,000 pairs of strings of 30 to 60 letters (the second
// one the first with random edits), by dynamic programming over two rows, the letters read as bytes.
use std::time::Instant;

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = (*seed * 16807) % 2147483647;
    *seed % n
}

#[inline(never)]
fn distance(s: &str, u: &str) -> i64 {
    let (s, u) = (s.as_bytes(), u.as_bytes());
    let n = u.len();
    let mut prev: Vec<i64> = (0..=n as i64).collect();
    let mut cur = vec![0i64; n + 1];
    for (i, &c) in s.iter().enumerate() {
        cur[0] = i as i64 + 1;
        for j in 0..n {
            cur[j + 1] = (prev[j + 1].min(cur[j]) + 1).min(prev[j] + if c == u[j] { 0 } else { 1 });
        }
        std::mem::swap(&mut prev, &mut cur);
    }
    prev[n]
}

fn letter(seed: &mut i64) -> char { (97 + rand(seed, 4)) as u8 as char }

fn pair(seed: &mut i64, len: i64) -> (String, String) {
    let mut s = String::new();
    let mut u = String::new();
    for _ in 0..len {
        s.push(letter(seed));
    }
    for &c in s.as_bytes() {
        let k = rand(seed, 10);
        if k == 0 {
            u.push(letter(seed));
        } else if k == 1 {
            u.push(c as char);
            u.push(letter(seed));
        } else if k != 2 {
            u.push(c as char);
        }
    }
    (s, u)
}

fn main() {
    let mut seed: i64 = 1001;
    let pairs: Vec<(String, String)> = (0..10000)
        .map(|_| {
            let len = 30 + rand(&mut seed, 31);
            pair(&mut seed, len)
        })
        .collect();
    let pairs = std::hint::black_box(pairs);
    let t = Instant::now();
    let mut n: i64 = 0;
    for p in &pairs {
        n += distance(&p.0, &p.1);
    }
    let ok = n == 108309 && distance("kitten", "sitting") == 3;
    println!("levenshtein {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
