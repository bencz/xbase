// bench/heap_sort.prg in Rust: heap sort in place (a binary max-heap built bottom-up, then sifted down) of 300,000
// integers from a linear congruential generator, 3 times with different seeds; the order and a weighted checksum
// are checked.
use std::hint::black_box;
use std::time::Instant;

// Moves a[i] down the heap a[1 ..= n] (1-based, a[0] unused) until neither child is larger.
fn sift_down(a: &mut [i64], mut i: usize, n: usize) {
    let x = a[i];
    loop {
        let mut c = i * 2;
        if c > n {
            break;
        }
        if c < n && a[c + 1] > a[c] {
            c += 1;
        }
        if a[c] <= x {
            break;
        }
        a[i] = a[c];
        i = c;
    }
    a[i] = x;
}

fn main() {
    let t = Instant::now();
    let n = black_box(300000usize);
    let mut a = vec![0i64; n + 1];
    let mut ok = true;
    let mut sum: i64 = 0;
    for rep in 1..=3i64 {
        let mut seed = rep + 10;
        for v in a[1..].iter_mut() {
            seed = seed * 16807 % 2147483647;
            *v = seed;
        }
        for i in (1..=n / 2).rev() {
            sift_down(&mut a, i, n);
        }
        for i in (2..=n).rev() {
            a.swap(1, i);
            sift_down(&mut a, 1, i - 1);
        }
        for i in 2..=n {
            if a[i - 1] > a[i] {
                ok = false;
            }
        }
        for i in 1..=n {
            sum += a[i] * ((i % 8) as i64 + 1);
        }
    }
    let ok = ok && sum == 4351668891394344;
    println!("heap_sort {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
