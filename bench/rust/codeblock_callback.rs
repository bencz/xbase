// bench/codeblock_callback.prg in Rust: functions that take a closure (&dyn Fn): Simpson's rule over 3 functions
// (1,000,000 intervals each), and an insertion sort of 3,000 numbers that compares through a closure, 10 times.
use std::time::Instant;

fn rand(seed: &mut i64, n: i64) -> i64 {
    *seed = (*seed * 16807) % 2147483647;
    *seed % n
}

#[inline(never)]
fn simpson(f: &dyn Fn(f64) -> f64, a: f64, b: f64, n: i64) -> f64 {
    let h = (b - a) / n as f64;
    let mut sum = f(a) + f(b);
    for i in 1..n {
        sum += f(a + i as f64 * h) * if i % 2 == 1 { 4.0 } else { 2.0 };
    }
    sum * h / 3.0
}

#[inline(never)]
fn insertion_sort(a: &mut [i64], less: &dyn Fn(i64, i64) -> bool) {
    for i in 1..a.len() {
        let x = a[i];
        let mut j = i;
        while j >= 1 && less(x, a[j - 1]) {
            a[j] = a[j - 1];
            j -= 1;
        }
        a[j] = x;
    }
}

fn main() {
    let mut seed: i64 = 1;
    let data: Vec<i64> = (0..3000).map(|_| rand(&mut seed, 100000)).collect();
    let data = std::hint::black_box(data);
    let t = Instant::now();
    let mut int = 0.0f64;
    int += simpson(&|x| x * x, 0.0, 3.0, 1000000);
    int += simpson(&|x| 1.0 / (1.0 + x * x), 0.0, 1.0, 1000000);
    int += simpson(&|x| x * x * x - 2.0 * x, -1.0, 2.0, 1000000);
    let mut check: i64 = 0;
    for r in 1..=10 {
        let mut a = data.clone();
        if r % 2 == 0 {
            insertion_sort(&mut a, &|x, y| x < y);
        } else {
            insertion_sort(&mut a, &|x, y| x > y);
        }
        check += a[0] + a[1499] * 2 + a[2999] * 3;
    }
    let ok = (int - 10.535398163397).abs() < 0.000000001 && check == 2991620;
    println!("codeblock_callback {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
