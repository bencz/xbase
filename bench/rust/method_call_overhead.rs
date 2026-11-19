// bench/method_call_overhead.prg in Rust: 5,000,000 rounds of calls with no argument, one and three arguments, an
// inline method and a method that calls another through self; on a struct whose type the compiler knows (it may
// resolve and inline the calls), then on one behind a trait object it cannot see through (black_box), so that each
// call is dispatched through the vtable.
use std::hint::black_box;
use std::time::Instant;

trait Calc {
    fn zero(&self) -> i64;
    fn one(&self, x: i64) -> i64;
    fn three(&self, x: i64, y: i64, z: i64) -> i64;
    fn twice(&self, x: i64) -> i64;
    fn chain(&self, x: i64) -> i64;
}

struct TCalc {
    base: i64,
}

impl Calc for TCalc {
    fn zero(&self) -> i64 { self.base }
    fn one(&self, x: i64) -> i64 { x }
    fn three(&self, x: i64, y: i64, z: i64) -> i64 { x + y + z - 5 }
    #[inline(always)]
    fn twice(&self, x: i64) -> i64 { x + x }
    fn chain(&self, x: i64) -> i64 { self.one(x) + self.one(x) + 1 }
}

fn main() {
    let t = Instant::now();
    let o = TCalc { base: 1 };
    let u: Box<dyn Calc> = black_box(Box::new(TCalc { base: 1 }));
    let mut n: i64 = 0;
    for i in 1..=black_box(5000000i64) {
        n += o.zero() + o.one(i) + o.three(i, 2, 3) + o.twice(i) + o.chain(i);
        n += u.zero() + u.one(i) + u.three(i, 2, 3) + u.twice(i) + u.chain(i);
    }
    // each round adds 2 * ( 6 * i + 2 )
    let ok = n == 150000050000000;
    println!("method_call_overhead {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
