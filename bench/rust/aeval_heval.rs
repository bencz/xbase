// bench/aeval_heval.prg in Rust: walks with closures, over a vector of 1,000,000 numbers (the closure given the value
// and its 1-based position) and a HashMap of 200,000 entries (given key and value), the closure adding to a captured
// local; 20 rounds. The walks take the closure as &mut dyn FnMut, as AEval() and HEval() take a block.
use std::collections::HashMap;
use std::time::Instant;

#[inline(never)]
fn aeval(a: &[i64], b: &mut dyn FnMut(i64, i64)) {
    for (i, x) in a.iter().enumerate() {
        b(*x, i as i64 + 1);
    }
}

#[inline(never)]
fn heval(h: &HashMap<i64, i64>, b: &mut dyn FnMut(i64, i64)) {
    for (k, v) in h {
        b(*k, *v);
    }
}

fn main() {
    let mut a = vec![0i64; 1000000];
    for (i, v) in a.iter_mut().enumerate() {
        *v = (i as i64 + 1) % 777;
    }
    let mut h: HashMap<i64, i64> = HashMap::new();
    for i in 1..=200000i64 {
        h.insert(i * 3, i % 555);
    }
    let t = Instant::now();
    let mut n: i64 = 0;
    for r in 1..=20i64 {
        aeval(&a, &mut |x, n_pos| n += x + if n_pos % 1000 == 0 { r } else { 0 });
        heval(&h, &mut |k, v| n += v + if k % 3000 == 0 { 1 } else { 0 });
    }
    let ok = n == 8867500260;
    println!("aeval_heval {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
