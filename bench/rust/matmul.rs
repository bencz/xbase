// bench/matmul.prg in Rust: the product of two 100 x 100 integer matrices, 20 times.
use std::time::Instant;

fn main() {
    let t = Instant::now();
    let n = std::hint::black_box(100usize);
    let mut a = vec![vec![0i64; n]; n];
    let mut b = vec![vec![0i64; n]; n];
    let mut c = vec![vec![0i64; n]; n];
    for i in 0..n {
        for j in 0..n {
            a[i][j] = ((i + j) % 7) as i64;
            b[i][j] = ((i * j) % 5) as i64;
        }
    }
    let mut total: i64 = 0;
    for _ in 0..20 {
        for i in 0..n {
            for j in 0..n {
                let mut s = 0;
                for k in 0..n { s += a[i][k] * b[k][j]; }
                c[i][j] = s;
                total += s;
            }
        }
    }
    std::hint::black_box(&c);
    println!("matmul {} {}", t.elapsed().as_millis(), if total == 20 * 4798200 { "ok" } else { "WRONG" });
}
