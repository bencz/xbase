// bench/ackermann.prg in Rust: Ackermann's function A(3, 9) (11 million calls, 4093 levels deep), 3 times.
use std::time::Instant;

fn ack(m: i64, n: i64) -> i64 {
    if m == 0 {
        n + 1
    } else if n == 0 {
        ack(m - 1, 1)
    } else {
        ack(m - 1, ack(m, n - 1))
    }
}

fn main() {
    let t = Instant::now();
    let mut ok = true;
    for _ in 0..3 {
        ok = ok && ack(std::hint::black_box(3), std::hint::black_box(9)) == 4093;
    }
    println!("ackermann {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
