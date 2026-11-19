// bench/radix_sort.prg in Rust: least-significant-digit radix sort, four passes of one byte each, of 300,000 31-bit
// integers from a linear congruential generator, between two Vecs, 10 times with different seeds; the order and a
// weighted checksum are checked.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n: usize = std::hint::black_box(300000);
    let mut a = vec![0i64; n];
    let mut b = vec![0i64; n];
    let mut cnt = [0usize; 256];
    let mut ok = true;
    let mut sum: i64 = 0;
    for rep in 1..=10i64 {
        let mut seed = rep + 30;
        for v in a.iter_mut() {
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
            *v = seed;
        }
        for d in 0..4 {
            let shift = d * 8;
            cnt.fill(0);
            for &x in &a {
                cnt[((x >> shift) & 0xFF) as usize] += 1;
            }
            let mut pos = 0;
            for c in cnt.iter_mut() {
                let x = *c;
                *c = pos;
                pos += x;
            }
            for &x in &a {
                let k = ((x >> shift) & 0xFF) as usize;
                b[cnt[k]] = x;
                cnt[k] += 1;
            }
            std::mem::swap(&mut a, &mut b);
        }
        for i in 1..n {
            if a[i - 1] > a[i] {
                ok = false;
            }
        }
        for (i, &x) in a.iter().enumerate() {
            sum += x * (((i as i64 + 1) & 7) + 1);
        }
    }
    let ok = ok && sum == 14501540638159929;
    println!("radix_sort {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
