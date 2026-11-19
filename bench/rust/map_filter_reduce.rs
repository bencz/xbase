// bench/map_filter_reduce.prg in Rust: map, filter and reduce written by hand over vectors with closures called
// through &dyn Fn: a vector of 1,000,000 numbers mapped, the result filtered and reduced to a sum; 8 rounds with
// other closures.
use std::time::Instant;

#[inline(never)]
fn map(a: &[i64], b: &dyn Fn(i64) -> i64) -> Vec<i64> {
    let mut out = vec![0i64; a.len()];
    for i in 0..a.len() {
        out[i] = b(a[i]);
    }
    out
}

#[inline(never)]
fn filter(a: &[i64], b: &dyn Fn(i64) -> bool) -> Vec<i64> {
    let mut out = Vec::new();
    for &x in a {
        if b(x) {
            out.push(x);
        }
    }
    out
}

#[inline(never)]
fn reduce(a: &[i64], b: &dyn Fn(i64, i64) -> i64, mut acc: i64) -> i64 {
    for &x in a {
        acc = b(acc, x);
    }
    acc
}

fn main() {
    let a: Vec<i64> = (1..=1000000i64).map(|i| i % 1000).collect();
    let a = std::hint::black_box(a);
    let t = Instant::now();
    let mut n: i64 = 0;
    for r in 1..=8i64 {
        n += reduce(&filter(&map(&a, &|x| x * r + 1), &|x| x % 3 == 0), &|acc, x| acc + x, 0);
    }
    let ok = n == 4492503000;
    println!("map_filter_reduce {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
