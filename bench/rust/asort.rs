// bench/asort.prg in Rust: the library's sort, 300,000 numbers ascending and 100,000 with a descending comparison,
// 5 times.
use std::time::Instant;

fn sort_both() -> bool {
    let mut a = vec![0i64; 300000];
    let mut b = vec![0i64; 100000];
    let mut x: i64 = 777;
    for v in a.iter_mut() { x = (x * 16807) % 2147483647; *v = x % 1000000; }
    x = 4242;
    for v in b.iter_mut() { x = (x * 16807) % 2147483647; *v = x % 1000000; }
    a.sort();
    b.sort_by(|p, q| q.cmp(p));
    let sum: i64 = a.iter().step_by(1000).sum();
    a[0] == 8 && a[149999] == 500673 && a[299999] == 999998 && sum == 149589926 && b[0] == 999994
        && b[49999] == 501282 && b[99999] == 8
}

fn main() {
    let t = Instant::now();
    let mut ok = true;
    for _ in 0..5 { ok = ok && sort_both(); }
    println!("asort {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
