// bench/qsort.prg in Rust: recursive quicksort over 200,000 Park-Miller numbers, 5 times.
use std::time::Instant;

fn qs(a: &mut [i64], mut lo: i64, mut hi: i64) {
    while lo < hi {
        let pivot = a[((lo + hi) / 2) as usize];
        let (mut i, mut j) = (lo, hi);
        while i <= j {
            while a[i as usize] < pivot { i += 1; }
            while a[j as usize] > pivot { j -= 1; }
            if i <= j {
                a.swap(i as usize, j as usize);
                i += 1;
                j -= 1;
            }
        }
        if j - lo < hi - i {
            qs(a, lo, j);
            lo = i;
        } else {
            qs(a, i, hi);
            hi = j;
        }
    }
}

fn sort_random() -> bool {
    let n: i64 = 200000;
    let mut a = vec![0i64; (n + 1) as usize]; // 1-based, as the xBase array
    let mut x: i64 = 12345;
    for i in 1..=n as usize {
        x = (x * 16807) % 2147483647;
        a[i] = x % 1000000;
    }
    qs(&mut a, 1, n);
    let sorted = (2..=n as usize).all(|i| a[i - 1] <= a[i]);
    let sum: i64 = (1..=n as usize).step_by(1000).map(|i| a[i]).sum();
    sorted && a[1] == 6 && a[100000] == 500294 && a[n as usize] == 999998 && sum == 99556483
}

fn main() {
    let t = Instant::now();
    let mut ok = true;
    for _ in 0..5 { ok = ok && sort_random(); }
    println!("qsort {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
