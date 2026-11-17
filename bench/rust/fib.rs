// bench/fib.prg in Rust: recursive calls, Fib( 37 ).
use std::time::Instant;

fn fib(n: i64) -> i64 {
    if n < 2 { n } else { fib(n - 1) + fib(n - 2) }
}

fn main() {
    let t = Instant::now();
    let r = fib(std::hint::black_box(37));
    println!("fib {} {}", t.elapsed().as_millis(), if r == 24157817 { "ok" } else { "WRONG" });
}
