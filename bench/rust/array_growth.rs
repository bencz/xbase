// bench/array_growth.prg in Rust: many vectors growing and shrinking: 300,000 vectors grown with push to 1 to 64
// elements, enlarged with resize to twice that, cut to half, an element inserted in front (insert, then the last
// one dropped, as AIns() keeps the length) and removed (remove, then a slot pushed at the end, as ADel()); one in 100
// kept to the end. The empty slots xBase fills with NIL hold 0.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let mut seed: i64 = 3;
    let mut rand = |n: i64| {
        seed = (seed * 16807) % 2147483647;
        seed % n
    };
    let mut kept: Vec<Vec<i64>> = Vec::new();
    let mut n: i64 = 0;
    for i in 1..=300000i64 {
        let mut a: Vec<i64> = Vec::new();
        let size = (rand(64) + 1) as usize;
        for j in 1..=size {
            a.push(j as i64 * 2);
        }
        a.resize(size * 2, 0);
        a[size * 2 - 1] = i % 1000;
        n += a[size * 2 - 1];
        a.truncate(size.div_ceil(2));
        a.insert(0, 0);
        a.pop();
        a[0] = size as i64;
        a.remove(0);
        a.push(0);
        let len = a.len();
        a[len - 1] = i % 7;
        n += len as i64 + a[len - 1] + if len > 1 { a[0] } else { 0 };
        let a = black_box(a);
        if i % 100 == 0 {
            kept.push(a);
        }
    }
    for a in &kept {
        n += a[0];
    }
    let ok = n == 156283982 && kept.len() == 3000;
    println!("array_growth {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
