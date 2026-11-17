// bench/strings.prg in Rust: Left, Upper, Len and At on 20,000,000 short strings (each a new String).
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let src = std::hint::black_box("abcdefghij");
    let mut total: i64 = 0;
    for i in 1..=20_000_000i64 {
        let w = src[..(i % 10 + 1) as usize].to_uppercase();
        total += w.len() as i64 + w.find('D').map_or(0, |p| p as i64 + 1);
    }
    println!("strings {} {}", t.elapsed().as_millis(), if total == 166000000 { "ok" } else { "WRONG" });
}
