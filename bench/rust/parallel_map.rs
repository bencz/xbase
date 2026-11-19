// bench/parallel_map.prg in Rust: 1,000,000 numbers (from a fixed seed) mapped to their Collatz step counts by 64
// threads, each mapping its slice into a Vec of its own; the slices are joined in order (copied into the result),
// and the result checked: its sum, its maximum and where it is first, a running hash of the whole.
use std::sync::Arc;
use std::thread;
use std::time::Instant;

fn steps(mut x: i64) -> i64 {
    let mut s = 0;
    while x != 1 {
        x = if x % 2 == 0 { x / 2 } else { 3 * x + 1 };
        s += 1;
    }
    s
}

fn map_slice(a_in: &[i64], from: usize, count: usize) -> Vec<i64> {
    let mut a = vec![0i64; count];
    for i in 0..count {
        a[i] = steps(a_in[from + i]);
    }
    a
}

fn main() {
    let n: usize = 1_000_000;
    let mut a_in = vec![0i64; n];
    let mut x: i64 = 777;
    for v in a_in.iter_mut() {
        x = x * 16807 % 2147483647;
        *v = x % 1000000 + 1;
    }
    let a_in = Arc::new(a_in);
    let mut a_out = vec![0i64; n];
    let t = Instant::now();
    let chunk = n / 64;
    let tasks: Vec<_> = (0..n)
        .step_by(chunk)
        .map(|from| {
            let a_in = Arc::clone(&a_in);
            thread::spawn(move || map_slice(&a_in, from, chunk))
        })
        .collect();
    let mut pos = 0;
    for task in tasks {
        let a = task.join().unwrap();
        a_out[pos..pos + a.len()].copy_from_slice(&a);
        pos += a.len();
    }
    let (mut sum, mut max, mut at, mut chk) = (0i64, 0i64, 0usize, 0i64);
    for (i, &x) in a_out.iter().enumerate() {
        sum += x;
        if x > max {
            max = x;
            at = i + 1;
        }
        chk = (chk * 31 + x) % 1000000007;
    }
    let ok = pos == n && sum == 131359344 && max == 524 && at == 237731 && chk == 68222219;
    println!("parallel_map {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
