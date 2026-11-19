// bench/counting_sort.prg in Rust: counting sort of 1,000,000 keys in 0 .. 65535 (the high bits of a linear
// congruential generator), stable through prefix sums into an output Vec, 6 times with different seeds; the order
// and a weighted checksum are checked.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n: usize = std::hint::black_box(1000000);
    let m: usize = 65536;
    let mut a = vec![0i64; n];
    let mut b = vec![0i64; n];
    let mut cnt = vec![0usize; m];
    let mut ok = true;
    let mut sum: i64 = 0;
    for rep in 1..=6i64 {
        let mut seed = rep + 20;
        for v in a.iter_mut() {
            seed = seed * 16807 % 2147483647;
            *v = seed / 32768;
        }
        cnt.fill(0);
        for &v in &a {
            cnt[v as usize] += 1;
        }
        let mut pos = 0;
        for c in cnt.iter_mut() {
            let k = *c;
            *c = pos;
            pos += k;
        }
        for &v in &a {
            let k = v as usize;
            b[cnt[k]] = v;
            cnt[k] += 1;
        }
        for i in 1..n {
            if b[i - 1] > b[i] {
                ok = false;
            }
        }
        for (i, &v) in b.iter().enumerate() {
            sum += v * ((i as i64 + 1) % 8 + 1);
        }
    }
    let ok = ok && sum == 884633960242;
    println!("counting_sort {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
