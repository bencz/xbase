// bench/oop.prg in Rust: an object behind a trait (dynamic dispatch), one method calling its parent's; method calls,
// variable reads and assignments in a loop of 3,000,000.
use std::time::Instant;

trait Adder { fn add(&mut self, x: i64); fn n(&self) -> i64; fn set_n(&mut self, v: i64); fn result(&self) -> i64; }
struct Base { total: i64 }
impl Base { fn add(&mut self, x: i64) { self.total += x; } }
struct Counter { base: Base, n: i64 }
impl Adder for Counter {
    fn add(&mut self, x: i64) { self.base.add(x); } // Super:Add( x )
    fn n(&self) -> i64 { self.n }
    fn set_n(&mut self, v: i64) { self.n = v; }
    fn result(&self) -> i64 { self.base.total + self.n }
}

fn main() {
    let t = Instant::now();
    let mut o: Box<dyn Adder> = std::hint::black_box(Box::new(Counter { base: Base { total: 0 }, n: 0 }));
    for i in 1..=3_000_000i64 {
        o.add(i % 7);
        let v = o.n() + 1;
        o.set_n(v);
    }
    let r = o.result();
    println!("oop {} {}", t.elapsed().as_millis(), if r == 11999997 { "ok" } else { "WRONG" });
}
