// bench/short_lived_objects.prg in Rust: 2,000,000 small vectors made on the heap (Box), each added to a running sum
// by a method that returns a new vector, so three objects are made and as many dropped per step.
use std::hint::black_box;
use std::time::Instant;

struct TVec {
    x: i64,
    y: i64,
}

impl TVec {
    #[inline(never)]
    fn new(x: i64, y: i64) -> Box<TVec> { black_box(Box::new(TVec { x, y })) }
    fn add(&self, o: &TVec) -> Box<TVec> { TVec::new(self.x + o.x, self.y + o.y) }
    fn scaled(&self, k: i64) -> Box<TVec> { TVec::new(self.x * k, self.y * k) }
    fn dot(&self, o: &TVec) -> i64 { self.x * o.x + self.y * o.y }
}

fn main() {
    let t = Instant::now();
    let mut s = TVec::new(0, 0);
    let mut n: i64 = 0;
    for i in 1..=2000000i64 {
        let v = TVec::new(i % 10, i % 7 - 3);
        s = s.add(&v);
        n += v.dot(&s.scaled(0)) + v.x;
    }
    let ok = s.x == 9000000 && s.y == -3 && n == 9000000;
    println!("short_lived_objects {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
