// bench/merge_sort.prg in Rust: recursive top-down merge sort of 300,000 integers from a linear congruential
// generator, through one work array, 3 times with different seeds; the order and a weighted checksum are checked.
use std::time::Instant;

// Sorts a[lo ..= hi]; w is the work array the halves merge through.
fn merge_sort(a: &mut [i64], w: &mut [i64], lo: usize, hi: usize) {
    if hi <= lo {
        return;
    }
    let mid = (lo + hi) / 2;
    merge_sort(a, w, lo, mid);
    merge_sort(a, w, mid + 1, hi);
    let (mut i, mut j, mut k) = (lo, mid + 1, lo);
    while i <= mid && j <= hi {
        if a[j] < a[i] {
            w[k] = a[j];
            j += 1;
        } else {
            w[k] = a[i];
            i += 1;
        }
        k += 1;
    }
    while i <= mid {
        w[k] = a[i];
        i += 1;
        k += 1;
    }
    while j <= hi {
        w[k] = a[j];
        j += 1;
        k += 1;
    }
    a[lo..=hi].copy_from_slice(&w[lo..=hi]);
}

fn main() {
    let t = Instant::now();
    let n: usize = 300000;
    let mut a = vec![0i64; n];
    let mut w = vec![0i64; n];
    let (mut ok, mut sum) = (true, 0i64);
    for rep in 1..=3i64 {
        let mut seed = rep;
        for v in a.iter_mut() {
            seed = seed * 16807 % 2147483647;
            *v = seed;
        }
        merge_sort(&mut a, &mut w, 0, n - 1);
        for i in 1..n {
            if a[i - 1] > a[i] {
                ok = false;
            }
        }
        for i in 1..=n {
            sum += a[i - 1] * ((i % 8) as i64 + 1);
        }
    }
    let ok = ok && sum == 4349496350073402;
    println!("merge_sort {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
