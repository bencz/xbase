// bench/longest_common_subsequence.prg in Rust: the length of the longest common subsequence of 600 pairs of 200 to
// 300 letters from a 4-letter alphabet (the second related to the first), by dynamic programming over two rows, the
// letters split into vectors first.
use std::time::Instant;

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = (*seed * 16807) % 2147483647;
    *seed % n
}

#[inline(never)]
fn lcs(s: &str, u: &str) -> i64 {
    let a_s: Vec<char> = s.chars().collect();
    let a_u: Vec<char> = u.chars().collect();
    let n = a_u.len();
    let mut prev = vec![0i64; n + 1];
    let mut cur = vec![0i64; n + 1];
    for &c in &a_s {
        for j in 0..n {
            cur[j + 1] = if c == a_u[j] { prev[j] + 1 } else { prev[j + 1].max(cur[j]) };
        }
        std::mem::swap(&mut prev, &mut cur);
    }
    prev[n]
}

fn pair(seed: &mut i64, len: i64) -> (String, String) {
    let mut s = String::new();
    let mut u = String::new();
    for _ in 0..len {
        s.push((65 + rand(seed, 4)) as u8 as char);
    }
    for &c in s.as_bytes() {
        if rand(seed, 4) != 0 {
            u.push(c as char);
        }
        if rand(seed, 5) == 0 {
            u.push((65 + rand(seed, 4)) as u8 as char);
        }
    }
    (s, u)
}

fn main() {
    let mut seed: i64 = 31;
    let pairs: Vec<(String, String)> = (0..600)
        .map(|_| {
            let len = 200 + rand(&mut seed, 101);
            pair(&mut seed, len)
        })
        .collect();
    let pairs = std::hint::black_box(pairs);
    let t = Instant::now();
    let mut n: i64 = 0;
    for p in &pairs {
        n += lcs(&p.0, &p.1);
    }
    let ok = n == 118188 && lcs("ABCBDAB", "BDCABA") == 4;
    println!("longest_common_subsequence {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
