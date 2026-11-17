// bench/generator.prg in Rust: an iterator of 3,000,000 numbers and a second one that takes the even ones from the
// first (boxed iterators: one call a step, as the xBase enumerators).
use std::time::Instant;

fn range(a: i64, b: i64) -> Box<dyn Iterator<Item = i64>> { Box::new(a..=b) }
fn evens(e: Box<dyn Iterator<Item = i64>>) -> Box<dyn Iterator<Item = i64>> { Box::new(e.filter(|x| x % 2 == 0)) }

fn main() {
    let t = Instant::now();
    let s: i64 = evens(range(1, std::hint::black_box(3_000_000))).sum();
    println!("generator {} {}", t.elapsed().as_millis(), if s == 2250001500000 { "ok" } else { "WRONG" });
}
