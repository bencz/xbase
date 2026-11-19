// bench/string_concatenation.prg in Rust: 3,000,000 short strings of seven parts picked by index from small tables,
// joined with + (each a new String), each one's length and last byte read.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let a = black_box(["alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta"]);
    let b = black_box(["red", "green", "blue", "cyan", "magenta"]);
    let mut n: i64 = 0;
    for i in 1..=3_000_000usize {
        let s = a[i % 7].to_string() + "-" + b[i % 5] + "/" + a[i % 3] + ":" + b[i % 4];
        let s = black_box(s);
        n += s.len() as i64 + i64::from(*s.as_bytes().last().unwrap());
    }
    let ok = n == 378692857;
    println!("string_concatenation {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
