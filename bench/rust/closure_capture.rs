// bench/closure_capture.prg in Rust: 500,000 counters made by a function, each a boxed closure holding its own copy of
// a local and a parameter, each called 4 times; then 2,000,000 calls of closures that share one captured local (a
// shared cell) and change it.
use std::cell::Cell;
use std::hint::black_box;
use std::rc::Rc;
use std::time::Instant;

#[inline(never)]
fn counter(step: i64) -> Box<dyn FnMut(i64) -> i64> {
    let mut count: i64 = 0;
    Box::new(move |x| {
        count += step + x;
        count
    })
}

fn main() {
    let t = Instant::now();
    let mut a: Vec<Box<dyn FnMut(i64) -> i64>> = Vec::with_capacity(500000);
    for i in 1..=500000i64 {
        a.push(counter(i % 10));
    }
    let mut n: i64 = 0;
    for j in 1..=4i64 {
        for b in a.iter_mut() {
            n += b(black_box(j));
        }
    }
    let shared = Rc::new(Cell::new(0i64));
    let s1 = Rc::clone(&shared);
    let s2 = Rc::clone(&shared);
    let inc: Box<dyn Fn(i64) -> i64> = black_box(Box::new(move |x| {
        s1.set(s1.get() + x);
        s1.get()
    }));
    let read: Box<dyn Fn() -> i64> = black_box(Box::new(move || s2.get() % 1000));
    for i in 1..=2000000i64 {
        inc(i % 7);
        n += read();
    }
    let ok = n == 1031500997 && shared.get() == 5999997;
    println!("closure_capture {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
