// bench/dgemm.prg in Rust: C := alpha A B + beta C on 250 x 250 matrices of doubles in flat row-major vectors (loops
// in i, k, j order), the inputs from a linear congruential generator, 3 times; the check is the sum and the trace of C.
use std::hint::black_box;
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n = black_box(250usize);
    let (alpha, beta) = (1.5f64, 0.5f64);
    let mut a = vec![0.0f64; n * n];
    let mut b = vec![0.0f64; n * n];
    let mut c = vec![0.0f64; n * n];
    let mut seed: i64 = 7;
    for i in 0..n * n {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        a[i] = seed as f64 / 1073741824.0 - 1.0;
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        b[i] = seed as f64 / 1073741824.0 - 1.0;
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        c[i] = seed as f64 / 1073741824.0 - 1.0;
    }
    for _ in 0..3 {
        for i in 0..n {
            let ci = &mut c[i * n..(i + 1) * n];
            for x in ci.iter_mut() {
                *x *= beta;
            }
            for k in 0..n {
                let aik = alpha * a[i * n + k];
                let bk = &b[k * n..(k + 1) * n];
                for j in 0..n {
                    ci[j] += aik * bk[j];
                }
            }
        }
    }
    let (mut sum, mut trace) = (0.0f64, 0.0f64);
    for i in 0..n {
        for j in 0..n {
            sum += c[i * n + j];
        }
        trace += c[i * n + i];
    }
    let ok = (sum + 145.62810535074).abs() < 0.000001 && (trace - 342.80906789597).abs() < 0.000001;
    println!("dgemm {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
