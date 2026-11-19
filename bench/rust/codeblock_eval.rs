// bench/codeblock_eval.prg in Rust: 10,000,000 calls of each of three boxed closures kept together (no argument, two
// arguments, one that reads a captured local), called through dyn Fn, the results summed.
use std::hint::black_box;
use std::time::Instant;

type Blocks = (Box<dyn Fn() -> i64>, Box<dyn Fn(i64, i64) -> i64>, Box<dyn Fn(i64) -> i64>);

fn blocks(n_base: i64) -> Blocks {
    (Box::new(|| 7), Box::new(|x, y| x * 2 + y), Box::new(move |x| x + n_base))
}

fn main() {
    let t = Instant::now();
    let b = black_box(blocks(3));
    let mut n: i64 = 0;
    for i in 1..=10_000_000i64 {
        n += (b.0)() + (b.1)(i % 100, 1) + (b.2)(i % 10);
    }
    let ok = n == 1145000000;
    println!("codeblock_eval {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
