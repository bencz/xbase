// bench/tak.prg in Rust: recursive calls with three arguments, the Takeuchi function Tak(24, 16, 8), 10 times.
use std::hint::black_box;
use std::time::Instant;

fn tak(x: i64, y: i64, z: i64) -> i64 {
    if y < x { tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y)) } else { z }
}

fn main() {
    let t = Instant::now();
    let mut ok = true;
    for _ in 0..10 {
        ok = ok && tak(black_box(24), black_box(16), black_box(8)) == 9;
    }
    println!("tak {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
